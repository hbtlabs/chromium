{
  'TOOLS': ['clang-newlib', 'glibc', 'pnacl', 'linux', 'mac'],
  'SEARCH': [
    '.',
    'pepper',
    '../third_party/newlib-extras',
    'include',
  ],
  'TARGETS': [
    {
      'DEFINES': ['NACL_IO_LOGGING=0'],
      'NAME' : 'nacl_io',
      'TYPE' : 'lib',
      'SOURCES' : [
        "devfs/dev_fs.cc",
        "devfs/jspipe_event_emitter.cc",
        "devfs/jspipe_node.cc",
        "devfs/tty_node.cc",
        "dir_node.cc",
        "event_emitter.cc",
        "event_listener.cc",
        "fifo_char.cc",
        "filesystem.cc",
        "fusefs/fuse_fs.cc",
        "fusefs/fuse_fs_factory.cc",
        "getdents_helper.cc",
        "googledrivefs/googledrivefs.cc",
        "googledrivefs/googledrivefs_node.cc",
        "hash.cc",
        "h_errno.c",
        "host_resolver.cc",
        "html5fs/html5_fs.cc",
        "html5fs/html5_fs_node.cc",
        "httpfs/http_fs.cc",
        "httpfs/http_fs_node.cc",
        "in6_addr.c",
        "jsfs/js_fs.cc",
        "jsfs/js_fs_node.cc",
        "kernel_handle.cc",
        "kernel_intercept.cc",
        "kernel_object.cc",
        "kernel_proxy.cc",
        "kernel_wrap_dummy.cc",
        "kernel_wrap_glibc.cc",
        "kernel_wrap_irt_ext.c",
        "kernel_wrap_newlib.cc",
        "kernel_wrap_win.cc",
        "log.c",
        "memfs/mem_fs.cc",
        "memfs/mem_fs_node.cc",
        "nacl_io.cc",
        "node.cc",
        "passthroughfs/passthrough_fs.cc",
        "passthroughfs/real_node.cc",
        "path.cc",
        "pepper_interface.cc",
        "pepper_interface_delegate.cc",
        "pipe/pipe_event_emitter.cc",
        "pipe/pipe_node.cc",
        "real_pepper_interface.cc",
        "socket/fifo_packet.cc",
        "socket/packet.cc",
        "socket/socket_node.cc",
        "socket/tcp_event_emitter.cc",
        "socket/tcp_node.cc",
        "socket/udp_event_emitter.cc",
        "socket/udp_node.cc",
        "socket/unix_event_emitter.cc",
        "socket/unix_node.cc",
        "stream/stream_event_emitter.cc",
        "stream/stream_fs.cc",
        "stream/stream_node.cc",
        "syscalls/access.c",
        "syscalls/chown.c",
        "syscalls/fchown.c",
        "syscalls/fcntl.c",
        "syscalls/ftruncate.c",
        "syscalls/futimes.c",
        "syscalls/getwd.c",
        "syscalls/ioctl.c",
        "syscalls/isatty.c",
        "syscalls/kill.c",
        "syscalls/killpg.c",
        "syscalls/lchown.c",
        "syscalls/mount.c",
        "syscalls/pipe.c",
        "syscalls/poll.c",
        "syscalls/realpath.c",
        "syscalls/select.c",
        "syscalls/sigaction.c",
        "syscalls/signal.c",
        "syscalls/sigpause.c",
        "syscalls/sigpending.c",
        "syscalls/sigset.c",
        "syscalls/sigsuspend.c",
        "syscalls/socket/accept.c",
        "syscalls/socket/bind.c",
        "syscalls/socket/connect.c",
        "syscalls/socket/freeaddrinfo.c",
        "syscalls/socket/gai_strerror.c",
        "syscalls/socket/getaddrinfo.c",
        "syscalls/socket/gethostbyname.c",
        "syscalls/socket/getnameinfo.c",
        "syscalls/socket/getpeername.c",
        "syscalls/socket/getsockname.c",
        "syscalls/socket/getsockopt.c",
        "syscalls/socket/herror.c",
        "syscalls/socket/hstrerror.c",
        "syscalls/socket/htonl.c",
        "syscalls/socket/htons.c",
        "syscalls/socket/inet_addr.c",
        "syscalls/socket/inet_aton.c",
        "syscalls/socket/inet_ntoa.c",
        "syscalls/socket/inet_ntop.cc",
        "syscalls/socket/inet_pton.c",
        "syscalls/socket/listen.c",
        "syscalls/socket/ntohl.c",
        "syscalls/socket/ntohs.c",
        "syscalls/socket/recv.c",
        "syscalls/socket/recvfrom.c",
        "syscalls/socket/recvmsg.c",
        "syscalls/socket/send.c",
        "syscalls/socket/sendmsg.c",
        "syscalls/socket/sendto.c",
        "syscalls/socket/setsockopt.c",
        "syscalls/socket/shutdown.c",
        "syscalls/socket/socket.c",
        "syscalls/socket/socketpair.c",
        "syscalls/termios/cfgetispeed.c",
        "syscalls/termios/cfgetospeed.c",
        "syscalls/termios/cfsetispeed.c",
        "syscalls/termios/cfsetospeed.c",
        "syscalls/termios/cfsetspeed.c",
        "syscalls/termios/tcflow.c",
        "syscalls/termios/tcflush.c",
        "syscalls/termios/tcdrain.c",
        "syscalls/termios/tcgetattr.c",
        "syscalls/termios/tcsendbreak.c",
        "syscalls/termios/tcsetattr.c",
        "syscalls/symlink.c",
        "syscalls/truncate.c",
        "syscalls/umask.c",
        "syscalls/umount.c",
        "syscalls/uname.c",
        "syscalls/utime.c",
      ],
    }
  ],
  'HEADERS': [
    {
      'FILES': [
        "char_node.h",
        "devfs/dev_fs.h",
        "devfs/jspipe_event_emitter.h",
        "devfs/jspipe_node.h",
        "devfs/tty_node.h",
        "dir_node.h",
        "error.h",
        "event_emitter.h",
        "event_listener.h",
        "fifo_char.h",
        "fifo_interface.h",
        "filesystem.h",
        "fs_factory.h",
        "fusefs/fuse_fs_factory.h",
        "fusefs/fuse_fs.h",
        "fuse.h",
        "getdents_helper.h",
        "googledrivefs/googledrivefs.h",
        "googledrivefs/googledrivefs_node.h",
        "hash.h",
        "host_resolver.h",
        "html5fs/html5_fs.h",
        "html5fs/html5_fs_node.h",
        "httpfs/http_fs.h",
        "httpfs/http_fs_node.h",
        "inode_pool.h",
        "ioctl.h",
        "jsfs/js_fs.h",
        "jsfs/js_fs_node.h",
        "nacl_abi_types.h",
        "kernel_handle.h",
        "kernel_intercept.h",
        "kernel_object.h",
        "kernel_proxy.h",
        "kernel_wrap.h",
        "kernel_wrap_real.h",
        "log.h",
        "memfs/mem_fs.h",
        "memfs/mem_fs_node.h",
        "nacl_io.h",
        "node.h",
        "osdirent.h",
        "osinttypes.h",
        "osmman.h",
        "ossignal.h",
        "ossocket.h",
        "osstat.h",
        "ostermios.h",
        "ostime.h",
        "ostypes.h",
        "osunistd.h",
        "osutime.h",
        "passthroughfs/passthrough_fs.h",
        "passthroughfs/real_node.h",
        "path.h",
        "pepper_interface_delegate.h",
        "pepper_interface_dummy.h",
        "pepper_interface.h",
        "pipe/pipe_event_emitter.h",
        "pipe/pipe_node.h",
        "real_pepper_interface.h",
        "socket/fifo_packet.h",
        "socket/packet.h",
        "socket/socket_node.h",
        "socket/tcp_event_emitter.h",
        "socket/tcp_node.h",
        "socket/udp_event_emitter.h",
        "socket/udp_node.h",
        "socket/unix_event_emitter.h",
        "socket/unix_node.h",
        "stream/stream_event_emitter.h",
        "stream/stream_fs.h",
        "stream/stream_node.h",
        "typed_fs_factory.h",
      ],
      'DEST': 'include/nacl_io',
    },
    {
      'FILES': [
        "arpa/inet.h",
        "memory.h",
        "netdb.h",
        "netinet/in.h",
        "netinet/tcp.h",
        "netinet6/in6.h",
        "poll.h",
        "sys/ioctl.h",
        "sys/mount.h",
        "sys/poll.h",
        "sys/select.h",
        "sys/socket.h",
        "sys/termios.h",
        "sys/time.h",
        "sys/utsname.h",
        "utime.h",
      ],
      'DEST': 'include/newlib',
    },
    {
      'FILES': [
        "bits/ioctls.h",
        "rpc/netdb.h",
        "sys/mount.h",
      ],
      'DEST': 'include/glibc',
    },
    {
      'FILES': [
        "arpa/inet.h",
        "memory.h",
        "netdb.h",
        "netinet/in.h",
        "netinet/tcp.h",
        "netinet6/in6.h",
        "poll.h",
        "sys/ioctl.h",
        "sys/mount.h",
        "sys/poll.h",
        "sys/select.h",
        "sys/socket.h",
        "sys/termios.h",
        "sys/time.h",
        "sys/utsname.h",
        "utime.h",
      ],
      'DEST': 'include/pnacl',
    },
    {
      'FILES': [
        "sys/mount.h",
      ],
      'DEST': 'include/mac',
    },
    {
      'FILES': [
        "poll.h",
        "sys/poll.h",
      ],
      'DEST': 'include/win',
    },
    {
      'FILES': [
        "all_interfaces.h",
        "define_empty_macros.h",
        "undef_macros.h",
      ],
      'DEST': 'include/nacl_io/pepper',
    }
  ],
  'DEST': 'src',
  'NAME': 'nacl_io',
}
