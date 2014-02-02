# Makefile
# author: byuu
# license: public domain

[A-Z] = A B C D E F G H I J K L M N O P Q R S T U V W X Y Z
[a-z] = a b c d e f g h i j k l m n o p q r s t u v w x y z
[0-9] = 0 1 2 3 4 5 6 7 8 9
[markup] = ` ~ ! @ \# $$ % ^ & * ( ) - _ = + [ { ] } \ | ; : ' " , < . > / ?
[all] = $([A-Z]) $([a-z]) $([0-9]) $([markup])
[space] :=
[space] +=

# platform detection
ifeq ($(platform),)
  uname := $(shell uname -s)
  ifeq ($(uname),)
    platform := windows
    delete = del $(subst /,\,$1)
  else ifneq ($(findstring Windows,$(uname)),)
    platform := windows
    delete = del $(subst /,\,$1)
  else ifneq ($(findstring CYGWIN,$(uname)),)
    platform := windows
    delete = del $(subst /,\,$1)
  else ifneq ($(findstring Darwin,$(uname)),)
    platform := macosx
    delete = rm -f $1
  else ifneq ($(findstring BSD,$(uname)),)
    platform := bsd
    delete = rm -f $1
  else
    platform := linux
    delete = rm -f $1
  endif
endif

# compiler detection
ifeq ($(compiler),)
  ifeq ($(platform),windows)
    compiler := g++
    flags := -fwrapv
    link :=
  else ifeq ($(platform),macosx)
    compiler := clang++
    flags := -fwrapv -w -stdlib=libc++
    link := -lc++ -lobjc
  else ifeq ($(platform),bsd)
    compiler := clang++
    flags := -fwrapv -w -I/usr/local/include
  else
    compiler := g++
    flags := -fwrapv
    link :=
  endif

  cflags := -x c -std=c99
  objcflags := -x objective-c -std=c99
  cppflags := -x c++ -std=c++11
  objcppflags := -x objective-c++ -std=c++11
endif

# cross-compilation support
ifeq ($(arch),x86)
  flags := -m32 $(flags)
  link := -m32 $(link)
endif

ifeq ($(prefix),)
  prefix := /usr/local
endif

# function rwildcard(directory, pattern)
rwildcard = \
  $(strip \
    $(filter $(if $2,$2,%), \
      $(foreach f, \
        $(wildcard $1*), \
        $(eval t = $(call rwildcard,$f/)) \
        $(if $t,$t,$f) \
      ) \
    ) \
  )

# function strtr(source, from, to)
strtr = \
  $(eval __temp := $1) \
  $(strip \
    $(foreach c, \
      $(join $(addsuffix :,$2),$3), \
      $(eval __temp := \
        $(subst $(word 1,$(subst :, ,$c)),$(word 2,$(subst :, ,$c)),$(__temp)) \
      ) \
    ) \
    $(__temp) \
  )

# function strupper(source)
strupper = $(call strtr,$1,$([a-z]),$([A-Z]))

# function strlower(source)
strlower = $(call strtr,$1,$([A-Z]),$([a-z]))

# function strlen(source)
strlen = \
  $(eval __temp := $(subst $([space]),_,$1)) \
  $(words \
    $(strip \
      $(foreach c, \
        $([all]), \
        $(eval __temp := \
          $(subst $c,$c ,$(__temp)) \
        ) \
      ) \
      $(__temp) \
    ) \
  )

# function streq(source)
streq = $(if $(filter-out xx,x$(subst $1,,$2)$(subst $2,,$1)x),,1)

# function strne(source)
strne = $(if $(filter-out xx,x$(subst $1,,$2)$(subst $2,,$1)x),1,)
