"""Apply the documented DST unpacked-frame fix to a private source copy."""
import pathlib
import shutil
import hashlib


def prepare(source, build):
    source, build = pathlib.Path(source).resolve(), pathlib.Path(build).resolve()
    old = '''        memcpy(frame->data[0], avpkt->data + 1, FFMIN(avpkt->size - 1, frame->nb_samples * channels));'''
    new = '''        /* TTPlayerDSD: dsd2pcm below consumes one DSD byte per float slot.
         * Keep the in-place conversion layout identical to the arithmetic path.
         * A short unpacked frame must not expose uninitialized buffer contents. */
        if (avpkt->size - 1 != frame->nb_samples * channels)
            return AVERROR_INVALIDDATA;
        for (i = 0; i < frame->nb_samples * channels; i++)
            frame->data[0][i * 4] = avpkt->data[i + 1];'''
    text = (source / 'libavcodec/dstdec.c').read_text(encoding='utf-8')
    if new in text:
        return source  # A corresponding-source archive already includes the fix.
    if text.count(old) != 1:
        raise RuntimeError('Unexpected FFmpeg DST source; review the unpacked-frame patch')
    target = build / 'dsd-patched-source'
    if target == source or source in target.parents:
        raise RuntimeError('DSD source copy must be outside the input source tree')
    identity=hashlib.sha256()
    for path in sorted(source.rglob('*')):
        if path.is_file() and '.git' not in path.parts and path.suffix not in ('.obj','.lib','.pyc'):
            identity.update(path.relative_to(source).as_posix().encode())
            identity.update(path.read_bytes())
    stamp=build/'dsd-source-id.txt'
    digest=str(source)+'\n'+identity.hexdigest()
    if not target.exists() or not stamp.exists() or stamp.read_text()!=digest:
        shutil.copytree(source, target, dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns('.git', '*.obj', '*.lib', '__pycache__'))
        stamp.write_text(digest)
    (target / 'libavcodec/dstdec.c').write_text(text.replace(old, new), encoding='utf-8')
    return target
