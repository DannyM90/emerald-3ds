"""Put the game on an SD card (or any folder laid out like one)."""

from __future__ import annotations

import os
import shutil
import stat
import string
import sys
import uuid
from pathlib import Path

from .errors import BuilderError

APP_DIR = Path("3ds") / "emerald3ds"


def find_sd_cards() -> list[Path]:
    """Mounted volumes that look like a 3DS SD card (they hold 'Nintendo 3DS').

    Only a suggestion: the builder never installs anywhere without the
    player choosing the destination."""
    candidates: list[Path] = []
    if os.name == "nt":
        roots = [Path("%s:\\" % letter) for letter in string.ascii_uppercase]
    elif sys.platform == "darwin":
        roots = list(Path("/Volumes").glob("*"))
    else:
        roots = [p for base in ("/media", "/run/media") for p in Path(base).glob("*/*")]
    for root in roots:
        try:
            if (root / "Nintendo 3DS").is_dir():
                candidates.append(root)
        except OSError:
            continue
    return candidates


_RETRY_HINT = ("Nothing already on the card was changed. Check that the card is still inserted, "
               "is not write-protected (the lock switch) and has free space, then try again.")


def _check_name(name) -> None:
    """Destination names must be plain file names: no folders, no drive letters."""
    if (not isinstance(name, str) or name in ("", ".", "..") or any(c in name for c in "/\\:\0")
            or Path(name).name != name):
        raise BuilderError("Refusing to install a file with an unsafe name: %r" % (name,),
                           code="install_failed")


def _create_temp(dest: Path, name: str) -> tuple[Path, int]:
    """A new, uniquely named file next to `name`; never reuses an existing file."""
    for _ in range(100):
        tmp = dest / ("%s.%s.partial" % (name, uuid.uuid4().hex[:12]))
        try:
            fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_BINARY", 0), 0o666)
        except FileExistsError:
            continue
        return tmp, fd
    raise FileExistsError("could not create a temporary file in %s" % dest)


def _remove(paths) -> None:
    for path in paths:
        try:
            path.unlink()
        except OSError:
            pass


def install(sd_root: Path, files: dict[str, Path], progress=None) -> Path:
    """Copy `files` ({name: source}) into <sd_root>/3ds/emerald3ds/.

    Every file is first copied (and synced) under a new, unique temporary name.
    Only when all of them are copied are they renamed into place, so a failed
    copy leaves the files already installed untouched and removes its own
    temporary files. The renames themselves are not one transaction: if one of
    them fails, the install can be left part old, part new, and the error says
    so. Behaviour when a card is pulled out or loses power is not verified."""
    sd_root = Path(sd_root)
    if not sd_root.is_dir():
        raise BuilderError("The destination folder does not exist:\n%s" % sd_root)
    for name in files:
        _check_name(name)
    dest = sd_root / APP_DIR
    need = 0
    for src in files.values():
        try:
            st = Path(src).stat()
        except OSError as exc:
            raise BuilderError("A file to install could not be read:\n%s\n(%s)" % (src, exc),
                               "Nothing was changed on the destination.", code="install_failed") from exc
        if not stat.S_ISREG(st.st_mode):
            raise BuilderError("A file to install is not a regular file:\n%s" % src,
                               "Nothing was changed on the destination.", code="install_failed")
        need += st.st_size
    try:
        free = shutil.disk_usage(sd_root).free
    except OSError:
        free = None   # free space unknown: skip this advisory check, the copy still reports errors
    if free is not None and free < need + (4 << 20):
        raise BuilderError("There is not enough free space on the destination (%d MiB needed)."
                           % ((need >> 20) + 4))
    try:
        dest.mkdir(parents=True, exist_ok=True)
    except OSError as exc:
        raise BuilderError("Could not create the folder %s:\n%s" % (dest, exc), _RETRY_HINT,
                           code="install_failed") from exc

    staged: list[tuple[Path, Path]] = []
    done = 0
    name = None
    try:
        for name, src in files.items():
            tmp, fd = _create_temp(dest, name)
            staged.append((tmp, dest / name))
            with os.fdopen(fd, "wb") as fout, Path(src).open("rb") as fin:
                while True:
                    chunk = fin.read(1 << 20)
                    if not chunk:
                        break
                    fout.write(chunk)
                    done += len(chunk)
                    if progress:
                        progress(done / max(need, 1))
                fout.flush()
                os.fsync(fout.fileno())
    except OSError as exc:
        _remove(tmp for tmp, _ in staged)
        raise BuilderError("Copying %s to %s failed:\n%s" % (name, dest, exc), _RETRY_HINT,
                           code="install_failed") from exc
    except BaseException:
        _remove(tmp for tmp, _ in staged)
        raise

    committed = 0
    try:
        for tmp, final in staged:
            os.replace(tmp, final)
            committed += 1
    except OSError as exc:
        _remove(tmp for tmp, _ in staged[committed:])
        if committed == 0:
            raise BuilderError("Putting %s in place in %s failed:\n%s" % (staged[0][1].name, dest, exc),
                               _RETRY_HINT, code="install_failed") from exc
        raise BuilderError("The install in %s is incomplete: %d of %d files were updated before "
                           "an error (%s)." % (dest, committed, len(staged), exc),
                           "The game may not start until it is installed again. Check the card, "
                           "then run the install again.", code="install_failed") from exc
    except BaseException:
        _remove(tmp for tmp, _ in staged[committed:])
        raise
    return dest
