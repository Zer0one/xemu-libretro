if get_option('libretro')
  specific_ss.add(files('renderer.c'))
else
  specific_ss.add([sdl, gloffscreen, files('renderer.c')])
endif
