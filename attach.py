import sys
import socket
import subprocess
import platform
import os

def main():
  print(sys.argv)

  id = sys.argv[1]
  ip = socket.gethostbyname_ex(socket.gethostname())[2][0]
  usbip = "/usr/lib/linux-tools/5.4.0-77-generic/usbip"

  get_busid_cmd = f"{usbip} list -r {ip} | grep {id} | cut -f 1 -d : | awk '{{$1=$1;print}}'"
  attach_cmd = f"{usbip} attach -r {ip} --busid $({get_busid_cmd})"
  print(get_busid_cmd)
  print(attach_cmd)


  is32bit = (platform.architecture()[0] == '32bit')
  system32 = os.path.join(os.environ['SystemRoot'], 'SysNative' if is32bit else 'System32')
  wsl = os.path.join(system32, 'wsl.exe')

  wsl_cmd = f'{wsl} -e /bin/bash -c'
  print(wsl_cmd)

  split_cmd = wsl_cmd.split() + [f"sudo {attach_cmd}"]
  print(split_cmd)

  subprocess.Popen(split_cmd)
  return

if __name__ == '__main__':
  main()
