import os
import ycm_core
from clang_helpers import PrepareClangFlags

def FlagsForFile(filename):
	return {
		'flags' : ['-ffast-math', '-Wall', '-msse2'],
		'do_cache' : True
	}
