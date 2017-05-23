# Attention, emacs, please use -*- mode: python -*-
# (even though this isn't actually Python code)

sources = [ 'vcowfs' ]


foreach ex : sources
    executable(ex, ex + '.c',
               include_directories: include_dirs,
               link_with: [ libfuse ],
               install: false)
endforeach
