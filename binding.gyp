{
    "targets": [
        {
            "target_name": "cryptocurrencyaddr",
            "sources": [
                "cryptocurrencyaddr.cc"
            ],
            'conditions': [
                ['OS=="win"',
                  {
                    'link_settings': {
                      'libraries': [
                        '-lws2_32.lib'
                      ],
                    }
                  }
                ]
              ],
            "cflags_cc": [
                "-std=c++0x"
            ],
        }
    ]
}
