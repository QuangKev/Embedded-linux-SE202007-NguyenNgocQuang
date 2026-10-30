#!/usr/bin/env python3
"""Build module, userspace tests and initramfs without sudo or host insmod."""
from pathlib import Path
import argparse
import gzip
import os
import shutil
import stat
import subprocess
import tempfile

here = Path(__file__).resolve().parent
lab = here.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--kernel', type=Path, default=Path.home()/'embedded_lab1/kernel/linux-5.15')
args = parser.parse_args()
kernel = args.kernel.resolve()
subprocess.run(['make', '-C', str(lab/'driver'), f'KDIR={kernel}', 'KCFLAGS=-Werror'], check=True)
module = lab/'driver/lab2_driver.ko'
root = lab/'rootfs/initramfs'
shutil.copy2(module, root/'lib/modules/5.15.0/lab2_driver.ko')
bin_dir = root/'usr/bin'
bin_dir.mkdir(parents=True, exist_ok=True)
for source, target, flags in [('test_lab2.c', 'test_lab2', ['-pthread']), ('lab2_ctl.c', 'lab2_ctl', [])]:
    subprocess.run(['arm-linux-gnueabihf-gcc', '-Wall', '-Wextra', '-Werror', '-O2', '-static',
                    *flags, str(here/source), '-o', str(bin_dir/target)], check=True)
shutil.copy2(here/'test_lab2.sh', bin_dir/'test_lab2.sh')
(bin_dir/'test_lab2.sh').chmod(0o755)
# Generate missing BusyBox links in the archive; avoid requiring root/mknod on Ubuntu.
links = {}
for name in 'sh mount cat chmod cp dmesg echo grep hostname ls mkdir rm sleep sync uname printf tail'.split():
    links['/bin/'+name] = 'busybox'
for name in 'init insmod rmmod lsmod mknod reboot'.split():
    links['/sbin/'+name] = '../bin/busybox'
for name in 'awk head stat wc timeout'.split():
    links['/usr/bin/'+name] = '../../bin/busybox'
entries = {'/': 'dir / 755 0 0'}
for folder in ['/dev', '/proc', '/sys', '/tmp', '/bin', '/sbin', '/usr', '/usr/bin']:
    entries[folder] = f'dir {folder} 755 0 0'
for f in sorted(root.rglob('*')):
    name = '/'+str(f.relative_to(root))
    mode = oct(stat.S_IMODE(f.lstat().st_mode))[2:]
    if f.is_symlink(): entries[name] = f'slink {name} {os.readlink(f)} {mode} 0 0'
    elif f.is_dir(): entries[name] = f'dir {name} {mode} 0 0'
    elif f.is_file():
        if any(c.isspace() for c in str(f)):
            raise SystemExit('gen_init_cpio requires a checkout path without spaces')
        entries[name] = f'file {name} {f} {mode} 0 0'
for name, target in links.items():
    entries.setdefault(name, f'slink {name} {target} 777 0 0')
entries['/dev/console'] = 'nod /dev/console 600 0 0 c 5 1'
entries['/dev/null'] = 'nod /dev/null 666 0 0 c 1 3'
with tempfile.NamedTemporaryFile('w') as spec:
    spec.write('\n'.join(entries[name] for name in sorted(entries))+'\n'); spec.flush()
    data = subprocess.check_output([str(kernel/'usr/gen_init_cpio'), spec.name])
output = lab/'output/initramfs_lab2.cpio.gz'
output.write_bytes(gzip.compress(data, mtime=0))
print('Built:', output, 'bytes:', output.stat().st_size)
