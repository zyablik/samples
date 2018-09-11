{
  'targets': [{
    'target_name': 'fbo_cube',
    'type': 'executable',
    'include_dirs': [
      '.',
      '<(DEPTH)/third_party/khronos/EGL/include',
      '<(DEPTH)/third_party/khronos/GLES2/include',
    ],
    'cflags': [
      '-DTARGET=ARM',
    ],
    'sources': [
      'fbo_cube.cpp',
      'runtime.c',
      'matrix.c',
      'shaders.c',
      'timer.cpp',
    ],
    'link_settings': {
      'libraries': [
        '-lEGL',
        '-lGLESv2',
      ],
    },
    'copies': [{
      'destination': '<(PRODUCT_DIR)',
      'files': [
        'assets',
      ],
    }],
  }],
}
