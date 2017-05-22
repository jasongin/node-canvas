{
  'conditions': [
    ['OS=="win"', {
      'variables': {
        'GTK_Root%': 'C:/Program Files/GTK', # Set the location of GTK all-in-one bundle
        'jpeg_root': 'C:/Program Files/libjpeg',
        'with_jpeg%': 'false',
        'with_gif%': 'false'
      }
    }, { # 'OS!="win"'
      'variables': {
        'with_jpeg%': '<!(./util/has_lib.sh jpeg)',
        'with_gif%': '<!(./util/has_lib.sh gif)'
      }
    }]
  ],
  'targets': [
    {
      'target_name': 'canvas-postbuild',
      'dependencies': ['canvas'],
      'conditions': [
        ['OS=="win"', {
          'copies': [{
            'destination': '<(PRODUCT_DIR)',
            'files': [
              '<(GTK_Root)/bin/zlib1.dll',
              '<(GTK_Root)/bin/libintl-8.dll',
              '<(GTK_Root)/bin/libpng14-14.dll',
              '<(GTK_Root)/bin/libpangocairo-1.0-0.dll',
              '<(GTK_Root)/bin/libpango-1.0-0.dll',
              '<(GTK_Root)/bin/libpangoft2-1.0-0.dll',
              '<(GTK_Root)/bin/libpangowin32-1.0-0.dll',
              '<(GTK_Root)/bin/libcairo-2.dll',
              '<(GTK_Root)/bin/libfontconfig-1.dll',
              '<(GTK_Root)/bin/libfreetype-6.dll',
              '<(GTK_Root)/bin/libglib-2.0-0.dll',
              '<(GTK_Root)/bin/libgobject-2.0-0.dll',
              '<(GTK_Root)/bin/libgmodule-2.0-0.dll',
              '<(GTK_Root)/bin/libgthread-2.0-0.dll',
              '<(GTK_Root)/bin/libexpat-1.dll',
              '<(jpeg_root)/bin/jpeg62.dll',
              '<(jpeg_root)/bin/turbojpeg.dll'
            ]
          }]
        }]
      ]
    },
    {
      'target_name': 'canvas',
      'include_dirs': ["<!(node -p \"require('node-addon-api').include\")"],
      'dependencies': ["<!(node -p \"require('node-addon-api').gyp\")"],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ],
      'xcode_settings': { 'GCC_ENABLE_CPP_EXCEPTIONS': 'YES' },
      'sources': [
        'src/Canvas.cc',
        'src/CanvasGradient.cc',
        'src/CanvasPattern.cc',
        'src/CanvasRenderingContext2d.cc',
        'src/color.cc',
        'src/Image.cc',
        'src/ImageData.cc',
        'src/register_font.cc',
        'src/init.cc'
      ],
      'conditions': [
        ['OS=="win"', {
          'libraries': [
            '-l<(GTK_Root)/lib/cairo.lib',
            '-l<(GTK_Root)/lib/libpng.lib',
            '-l<(GTK_Root)/lib/pangocairo-1.0.lib',
            '-l<(GTK_Root)/lib/pango-1.0.lib',
            '-l<(GTK_Root)/lib/freetype.lib',
            '-l<(GTK_Root)/lib/glib-2.0.lib',
            '-l<(GTK_Root)/lib/gobject-2.0.lib'
          ],
          'include_dirs': [
            '<(GTK_Root)/include',
            '<(GTK_Root)/include/cairo',
            '<(GTK_Root)/include/pango-1.0',
            '<(GTK_Root)/include/glib-2.0',
            '<(GTK_Root)/include/freetype2',
            '<(GTK_Root)/lib/glib-2.0/include',
            '<(jpeg_root)/include'
          ],
          'defines': [
            '_USE_MATH_DEFINES' # for M_PI
          ],
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'WarningLevel': 4,
                  'ExceptionHandling': 1,
                  'DisableSpecificWarnings': [4100, 4127, 4201, 4244, 4267, 4506, 4611, 4714, 4512]
                }
              }
            },
            'Release': {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'WarningLevel': 4,
                  'ExceptionHandling': 1,
                  'DisableSpecificWarnings': [4100, 4127, 4201, 4244, 4267, 4506, 4611, 4714, 4512]
                }
              }
            }
          }
        }, { # 'OS!="win"'
          'libraries': [
            '<!@(pkg-config pixman-1 --libs)',
            '<!@(pkg-config cairo --libs)',
            '<!@(pkg-config libpng --libs)',
            '<!@(pkg-config pangocairo --libs)',
            '<!@(pkg-config freetype2 --libs)'
          ],
          'include_dirs': [
            '<!@(pkg-config cairo --cflags-only-I | sed s/-I//g)',
            '<!@(pkg-config libpng --cflags-only-I | sed s/-I//g)',
            '<!@(pkg-config pangocairo --cflags-only-I | sed s/-I//g)',
            '<!@(pkg-config freetype2 --cflags-only-I | sed s/-I//g)'
          ]
        }],
        ['with_jpeg=="true"', {
          'defines': [
            'HAVE_JPEG'
          ],
          'conditions': [
            ['OS=="win"', {
              'libraries': [
                '-l<(jpeg_root)/lib/jpeg.lib'
              ]
            }, {
              'libraries': [
                '-ljpeg'
              ]
            }]
          ]
        }],
        ['with_gif=="true"', {
          'defines': [
            'HAVE_GIF'
          ],
          'conditions': [
            ['OS=="win"', {
              'libraries': [
                '-l<(GTK_Root)/lib/gif.lib'
              ]
            }, {
              'libraries': [
                '-lgif'
              ]
            }]
          ]
        }]
      ]
    }
  ]
}
