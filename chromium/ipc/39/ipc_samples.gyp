{
   'target_defaults': {
      'dependencies': [
        '../../src/3rdparty/chromium/ipc/ipc.gyp:ipc',
        '../../src/3rdparty/chromium/base/base.gyp:base',
#       '../../src/3rdparty/chromium/ui/surface/surface.gyp:surface'
      ],
      'include_dirs': [
          '.',
          '../../src/3rdparty/chromium'
      ],
      'ldflags!': [
          '--sysroot=/home/qmobile/qmobile/qt/qtwebengine/src/3rdparty/chromium/build/linux/debian_wheezy_arm-sysroot'
      ],
      'ldflags': [
        '-L/home/qmobile/out/target/product/hammerhead/system/lib/ -lgnustl_shared',
        '--sysroot=/home/qmobile/prebuilts/ndk/8/platforms/android-14/arch-arm'
      ],
      'libraries': ['-lcutils'],
      'libraries!': ['-lrt', '-ldl', '-licui18n', '-licuuc', '-licudata'],
   },

    'targets': [
    {
      'target_name': 'ipc_hello_world_client_and_server',
      'type': 'executable',
      'sources': [
        'ipc_hello_world_client_and_server.cc',
      ],
    },
    {
      'target_name': 'ipc_hello_world_client',
      'type': 'executable',
      'sources': [
        'ipc_hello_world_client.cc',
      ],
    },
    {
      'target_name': 'ipc_hello_world_server',
      'type': 'executable',
      'sources': [
        'ipc_hello_world_server.cc',
      ],
    },
    ]
}
