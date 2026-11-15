# Builds HelloGui_0xE6205EE3.mif from ..\gfx\qgn_menu_hellogui.svg
# Recipe lines MUST start with a TAB character.

ifeq (WINS,$(findstring WINS, $(PLATFORM)))
ZDIR=$(EPOCROOT)epoc32\release\$(PLATFORM)\$(CFG)\Z
else
ZDIR=$(EPOCROOT)epoc32\data\z
endif

TARGETDIR=$(ZDIR)esourcepps
ICONTARGETFILENAME=$(TARGETDIR)\HelloGui_0xE6205EE3.mif
ICONDIR=..\gfx

do_nothing :
	@rem do_nothing

MAKMAKE : do_nothing
BLD : do_nothing
CLEAN : do_nothing
LIB : do_nothing
CLEANLIB : do_nothing

RESOURCE : $(ICONTARGETFILENAME)

$(ICONTARGETFILENAME) : $(ICONDIR)\qgn_menu_hellogui.svg
	mifconv $(ICONTARGETFILENAME) /X /c32 $(ICONDIR)\qgn_menu_hellogui.svg

FREEZE : do_nothing
SAVESPACE : do_nothing
RELEASABLES :
	@echo $(ICONTARGETFILENAME)
FINAL : do_nothing
