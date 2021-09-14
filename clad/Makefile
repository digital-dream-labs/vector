# CLAD makefile by Pauley

CLAD_BASE_DIR=../victor-clad/tools/message-buffers
CLAD_EMITTER_DIR=$(CLAD_BASE_DIR)/emitters
CLAD_LIB_DIR=$(CLAD_BASE_DIR)/clad
CLAD_DEPENDENCIES=$(CLAD_LIB_DIR)/*.py
CLAD_CPP=$(CLAD_EMITTER_DIR)/CPP_emitter.py
CLAD_CSHARP=$(CLAD_EMITTER_DIR)/CSharp_emitter.py
CLAD_PYTHON=$(CLAD_EMITTER_DIR)/Python_emitter.py
CLAD_CPP_DECL=../robot/clad/cozmo_CPP_declarations_emitter.py
CLAD_CPP_SWITCH=./cozmo_CPP_switch_emitter.py
CLAD_HASH=$(CLAD_EMITTER_DIR)/ASTHash_emitter.py
CLAD_CPP_FLAGS=--output-union-helper-constructors --output-json

OUTPUT_DIR:=../generated/clad/
OUTPUT_DIR_CSHARP=../generated/cladCsharp/
OUTPUT_DIR_PYTHON=../generated/cladPython/
OUTPUT_DIR_ENGINE:=$(OUTPUT_DIR)engine/

OUTPUT_DIR_CTI_VISION:=../coretech/generated/clad/vision/
OUTPUT_DIR_CTI_COMMON:=../coretech/generated/clad/common/

INPUT_UTIL_DIR=../lib/util/source/anki/clad

# Generate python for util, since it only generates C++ by default
INPUT_UTIL_FILES=$(shell cd $(INPUT_UTIL_DIR); find . -type f -iname '*.clad')
OUTPUT_UTIL_PYTHON=$(patsubst %.clad, %.py, $(INPUT_UTIL_FILES))

INPUT_ENGINE_DIR=./src/
INPUT_ENGINE_FILES=$(shell cd $(INPUT_ENGINE_DIR); find . -type f -iname '*.clad')
OUTPUT_ENGINE_CPP=$(patsubst %.clad, %.cpp, $(INPUT_ENGINE_FILES))
OUTPUT_ENGINE_H=$(patsubst %.clad, %.h, $(INPUT_ENGINE_FILES))
OUTPUT_ENGINE_H_AND_CPP=$(OUTPUT_ENGINE_H) $(OUTPUT_ENGINE_CPP)
OUTPUT_ENGINE_CSHARP=$(patsubst %.clad, %.cs, $(INPUT_ENGINE_FILES))
OUTPUT_ENGINE_PYTHON=$(patsubst %.clad, %.py, $(INPUT_ENGINE_FILES))
ENGINE_GAME_TO_ENGINE_UNION=clad/externalInterface/messageGameToEngine
ENGINE_ENGINE_TO_GAME_UNION=clad/externalInterface/messageEngineToGame
OUTPUT_ENGINE_LST=$(OUTPUT_DIR)engine.lst

INPUT_VIZ_DIR=./vizSrc/
INPUT_VIZ_FILES=$(shell cd $(INPUT_VIZ_DIR); find . -type f -iname '*.clad')
OUTPUT_VIZ_CPP=$(patsubst %.clad, %.cpp, $(INPUT_VIZ_FILES))
OUTPUT_VIZ_H=$(patsubst %.clad, %.h, $(INPUT_VIZ_FILES))
OUTPUT_VIZ_H_AND_CPP=$(OUTPUT_VIZ_H) $(OUTPUT_VIZ_CPP)
OUTPUT_VIZ_CSHARP=$(patsubst %.clad, %.cs, $(INPUT_VIZ_FILES))
OUTPUT_VIZ_PYTHON=$(patsubst %.clad, %.py, $(INPUT_VIZ_FILES))
OUTPUT_VIZ_LST=$(OUTPUT_DIR)viz.lst

# Gather list of robot clad files
INPUT_ROBOT_DIR=../robot/clad/src/
INPUT_ROBOT_FILES=$(shell cd $(INPUT_ROBOT_DIR); find . -type f -iname '*.clad')

# Determine which clad files are shared with outside world
INPUT_ROBOT_SHARED_FILES=$(shell cd $(INPUT_ROBOT_DIR); grep -il ROBOT_CLAD_SHARED $(INPUT_ROBOT_FILES))
INPUT_ROBOT_NOT_SHARED_FILES=$(shell cd $(INPUT_ROBOT_DIR); grep -iL ROBOT_CLAD_SHARED $(INPUT_ROBOT_FILES))

# Generate cpp and h for each robot clad
OUTPUT_ROBOT_CPP=$(patsubst %.clad, %.cpp, $(INPUT_ROBOT_FILES))
OUTPUT_ROBOT_H=$(patsubst %.clad, %.h, $(INPUT_ROBOT_FILES))
OUTPUT_ROBOT_H_AND_CPP=$(OUTPUT_ROBOT_H) $(OUTPUT_ROBOT_CPP)

# Generate cs and py for each shared robot clad
OUTPUT_ROBOT_CSHARP=$(patsubst %.clad, %.cs, $(INPUT_ROBOT_SHARED_FILES))
OUTPUT_ROBOT_PYTHON=$(patsubst %.clad, %.py, $(INPUT_ROBOT_SHARED_FILES))

ROBOT_ENGINE_TO_ROBOT_UNION=clad/robotInterface/messageEngineToRobot
ROBOT_ROBOT_TO_ENGINE_UNION=clad/robotInterface/messageRobotToEngine
OUTPUT_ROBOT_LST=$(OUTPUT_DIR)robot.lst

# TODO: Move this to its own Makefile/build inside coretech-internal
INPUT_CTI_VISION_DIR=../coretech/vision/clad/src/
INPUT_CTI_VISION_FILES=$(shell cd $(INPUT_CTI_VISION_DIR); find . -type f -iname '*.clad')
OUTPUT_CTI_VISION_CPP=$(patsubst %.clad, %.cpp, $(INPUT_CTI_VISION_FILES))
OUTPUT_CTI_VISION_H=$(patsubst %.clad, %.h, $(INPUT_CTI_VISION_FILES))
OUTPUT_CTI_VISION_H_AND_CPP=$(OUTPUT_CTI_VISION_H) $(OUTPUT_CTI_VISION_CPP)
OUTPUT_CTI_VISION_CSHARP=$(patsubst %.clad, %.cs, $(INPUT_CTI_VISION_FILES))
OUTPUT_CTI_VISION_PYTHON=$(patsubst %.clad, %.py, $(INPUT_CTI_VISION_FILES))
OUTPUT_CTI_VISION_LST=$(OUTPUT_DIR_CTI_VISION)vision.lst
OUTPUT_CTI_VISION_ENGINE_LST=$(OUTPUT_DIR)vision.lst

INPUT_CTI_COMMON_DIR=../coretech/common/clad/src/
INPUT_CTI_COMMON_FILES=$(shell cd $(INPUT_CTI_COMMON_DIR); find . -type f -iname '*.clad')
OUTPUT_CTI_COMMON_CPP=$(patsubst %.clad, %.cpp, $(INPUT_CTI_COMMON_FILES))
OUTPUT_CTI_COMMON_H=$(patsubst %.clad, %.h, $(INPUT_CTI_COMMON_FILES))
OUTPUT_CTI_COMMON_H_AND_CPP=$(OUTPUT_CTI_COMMON_H) $(OUTPUT_CTI_COMMON_CPP)
OUTPUT_CTI_COMMON_CSHARP=$(patsubst %.clad, %.cs, $(INPUT_CTI_COMMON_FILES))
OUTPUT_CTI_COMMON_PYTHON=$(patsubst %.clad, %.py, $(INPUT_CTI_COMMON_FILES))
OUTPUT_CTI_COMMON_LST=$(OUTPUT_DIR_CTI_COMMON)common.lst
OUTPUT_CTI_COMMON_ENGINE_LST=$(OUTPUT_DIR)common.lst

vpath %.clad $(INPUT_ENGINE_DIR):$(INPUT_VIZ_DIR):$(INPUT_ROBOT_DIR):$(INPUT_CTI_VISION_DIR):$(INPUT_CTI_COMMON_DIR):$(INPUT_UTIL_DIR)
vpath %.h $(OUTPUT_DIR_ENGINE)/
vpath %.cpp $(OUTPUT_DIR_ENGINE)/
vpath %.cs $(OUTPUT_DIR_CSHARP)/
vpath %.py $(OUTPUT_DIR_PYTHON)/
vpath %.h $(OUTPUT_DIR_CTI_VISION)/
vpath %.cpp $(OUTPUT_DIR_CTI_VISION)/
vpath %.cpp $(OUTPUT_DIR_CTI_COMMON)/
vpath %.cs $(OUTPUT_DIR_CTI_VISION)/
vpath %.h $(OUTPUT_DIR_CTI_VISION)/
vpath %.h $(OUTPUT_DIR_CTI_COMMON)/

.PHONY: show clean

all: cpp python 

# C++
cpp: engineCpp robotCpp vizCpp ctiVisionCpp ctiCommonCpp cppListFiles
cppListFiles: $(OUTPUT_VIZ_LST) $(OUTPUT_ENGINE_LST) $(OUTPUT_ROBOT_LST) $(OUTPUT_CTI_VISION_LST) $(OUTPUT_CTI_VISION_ENGINE_LST) $(OUTPUT_CTI_COMMON_LST) $(OUTPUT_CTI_COMMON_ENGINE_LST)
robotCpp: $(OUTPUT_ROBOT_CPP)
engineCpp: $(OUTPUT_ENGINE_H_AND_CPP)
vizCpp: $(OUTPUT_VIZ_H_AND_CPP)
ctiVisionCpp: $(OUTPUT_CTI_VISION_H_AND_CPP)
ctiCommonCpp: $(OUTPUT_CTI_COMMON_H_AND_CPP)

# C-sharp
csharp: engineCsharp robotCsharp vizCsharp ctiCommonCsharp ctiVisionCsharp 
robotCsharp: $(OUTPUT_ROBOT_CSHARP)
engineCsharp: $(OUTPUT_ENGINE_CSHARP)
vizCsharp: $(OUTPUT_VIZ_CSHARP)
ctiVisionCsharp: $(OUTPUT_CTI_VISION_CSHARP)
ctiCommonCsharp: $(OUTPUT_CTI_COMMON_CSHARP)

# Python
python: enginePython robotPython vizPython ctiVisionPython ctiCommonPython utilPython
robotPython: $(OUTPUT_ROBOT_PYTHON)
enginePython: $(OUTPUT_ENGINE_PYTHON)
utilPython: $(OUTPUT_UTIL_PYTHON)
vizPython: $(OUTPUT_VIZ_PYTHON)
ctiVisionPython: $(OUTPUT_CTI_VISION_PYTHON)
ctiCommonPython: $(OUTPUT_CTI_COMMON_PYTHON)


$(OUTPUT_VIZ_H_AND_CPP): $(INPUT_VIZ_FILES) $(CLAD_DEPENDENCIES) $(CLAD_CPP) $(CLAD_CPP_DECL) $(CLAD_CPP_SWITCH)
	if test -f $(OUTPUT_DIR_ENGINE)/$*.h; then chmod -f 777 $(OUTPUT_DIR_ENGINE)/$*.h; fi
	if test -f $(OUTPUT_DIR_ENGINE)/$*.cpp; then chmod -f 777 $(OUTPUT_DIR_ENGINE)/$*.cpp; fi
	$(CLAD_CPP) $(CLAD_CPP_FLAGS) -C $(INPUT_VIZ_DIR) -I $(INPUT_ROBOT_DIR) $(INPUT_ENGINE_DIR) $(INPUT_UTIL_DIR) -o $(OUTPUT_DIR_ENGINE)/ $*.clad
	$(CLAD_CPP_DECL) -C $(INPUT_VIZ_DIR) -I $(INPUT_ROBOT_DIR) $(INPUT_ENGINE_DIR) $(INPUT_UTIL_DIR) -o $(OUTPUT_DIR_ENGINE)/ $*.clad
	$(CLAD_CPP_SWITCH) -C $(INPUT_VIZ_DIR) -I $(INPUT_ROBOT_DIR) $(INPUT_ENGINE_DIR) $(INPUT_UTIL_DIR) -o $(OUTPUT_DIR_ENGINE)/ $*.clad
	chmod -f 555 $(OUTPUT_DIR_ENGINE)/$*.h
	chmod -f 555 $(OUTPUT_DIR_ENGINE)/$*.cpp

%_hash.h : %.clad $(CLAD_DEPENDENCIES) $(CLAD_HASH)
	if ! test -d $(dir $(OUTPUT_DIR_ENGINE)$@); then mkdir -p $(dir $(OUTPUT_DIR_ENGINE)$@); fi
	if test -f $(OUTPUT_DIR_ENGINE)$@; then chmod -f 777 $(OUTPUT_DIR_ENGINE)$@; fi
	python $(CLAD_HASH) -I $(INPUT_ROBOT_DIR) $(INPUT_ENGINE_DIR) $(INPUT_CTI_VISION_DIR) $(INPUT_CTI_COMMON_DIR) $(INPUT_UTIL_DIR) -o $(OUTPUT_DIR_ENGINE)$@ $<
	chmod -f 555 $(OUTPUT_DIR_ENGINE)$@
	
%_hash.py : %.clad $(CLAD_DEPENDENCIES) $(CLAD_HASH)
	if ! test -d $(dir $(OUTPUT_DIR_PYTHON)$@); then mkdir -p $(dir $(OUTPUT_DIR_PYTHON)$@); fi
	if test -f $(OUTPUT_DIR_PYTHON)$@; then chmod -f 777 $(OUTPUT_DIR_PYTHON)$@; fi
	python $(CLAD_HASH) -I $(INPUT_ROBOT_DIR) $(INPUT_ENGINE_DIR) $(INPUT_CTI_VISION_DIR) $(INPUT_CTI_COMMON_DIR) $(INPUT_UTIL_DIR) -o $(OUTPUT_DIR_PYTHON)$@ $<
	chmod -f 555 $(OUTPUT_DIR_PYTHON)$@
	
%_hash.cs : %.clad $(CLAD_DEPENDENCIES) $(CLAD_HASH)
	if ! test -d $(dir $(OUTPUT_DIR_CSHARP)$@); then mkdir -p $(dir $(OUTPUT_DIR_CSHARP)$@); fi
	if test -f $(OUTPUT_DIR_CSHARP)$@; then chmod -f 777 $(OUTPUT_DIR_CSHARP)$@; fi
	python $(CLAD_HASH) -I $(INPUT_ROBOT_DIR) $(INPUT_ENGINE_DIR) $(INPUT_CTI_VISION_DIR) $(INPUT_CTI_COMMON_DIR) $(INPUT_UTIL_DIR) -o $(OUTPUT_DIR_CSHARP)$@ $<
	chmod -f 555 $(OUTPUT_DIR_CSHARP)$@

$(OUTPUT_ROBOT_H_AND_CPP): $(INPUT_ROBOT_FILES) $(CLAD_DEPENDENCIES) $(CLAD_CPP) $(CLAD_CPP_DECL) $(CLAD_CPP_SWITCH) $(ROBOT_ENGINE_TO_ROBOT_UNION)_hash.h $(ROBOT_ROBOT_TO_ENGINE_UNION)_hash.h
	if test -f $(OUTPUT_DIR_ENGINE)/$*.h; then chmod -f 777 $(OUTPUT_DIR_ENGINE)/$*.h; fi
	if test -f $(OUTPUT_DIR_ENGINE)/$*.cpp; then chmod -f 777 $(OUTPUT_DIR_ENGINE)/$*.cpp; fi
	$(CLAD_CPP) $(CLAD_CPP_FLAGS) -C $(INPUT_ROBOT_DIR) -o $(OUTPUT_DIR_ENGINE)/ $*.clad
	$(CLAD_CPP_DECL) -C $(INPUT_ROBOT_DIR) -o $(OUTPUT_DIR_ENGINE)/ $*.clad
	$(CLAD_CPP_SWITCH) -C $(INPUT_ROBOT_DIR) -o $(OUTPUT_DIR_ENGINE)/ $*.clad
	chmod -f 555 $(OUTPUT_DIR_ENGINE)/$*.h
	chmod -f 555 $(OUTPUT_DIR_ENGINE)/$*.cpp

$(OUTPUT_ENGINE_H_AND_CPP): $(INPUT_ENGINE_FILES) $(CLAD_DEPENDENCIES) $(CLAD_CPP) $(CLAD_CPP_DECL) $(CLAD_CPP_SWITCH) $(ENGINE_GAME_TO_ENGINE_UNION)_hash.h $(ENGINE_ENGINE_TO_GAME_UNION)_hash.h
	if test -f $(OUTPUT_DIR_ENGINE)/$*.h; then chmod -f 777 $(OUTPUT_DIR_ENGINE)/$*.h; fi
	if test -f $(OUTPUT_DIR_ENGINE)/$*.cpp; then chmod -f 777 $(OUTPUT_DIR_ENGINE)/$*.cpp; fi
	$(CLAD_CPP) $(CLAD_CPP_FLAGS) -C $(INPUT_ENGINE_DIR) -I $(INPUT_ROBOT_DIR) $(INPUT_CTI_VISION_DIR) $(INPUT_CTI_COMMON_DIR) $(INPUT_UTIL_DIR) -o $(OUTPUT_DIR_ENGINE)/ $*.clad
	$(CLAD_CPP_DECL) -C $(INPUT_ENGINE_DIR) -I $(INPUT_ROBOT_DIR) $(INPUT_CTI_VISION_DIR) $(INPUT_CTI_COMMON_DIR) $(INPUT_UTIL_DIR) -o $(OUTPUT_DIR_ENGINE)/ $*.clad
	$(CLAD_CPP_SWITCH) -C $(INPUT_ENGINE_DIR) -I $(INPUT_ROBOT_DIR) $(INPUT_CTI_VISION_DIR) $(INPUT_CTI_COMMON_DIR) $(INPUT_UTIL_DIR) -o $(OUTPUT_DIR_ENGINE)/ $*.clad
	chmod -f 555 $(OUTPUT_DIR_ENGINE)/$*.h
	chmod -f 555 $(OUTPUT_DIR_ENGINE)/$*.cpp
	
$(OUTPUT_CTI_VISION_H_AND_CPP): $(INPUT_CTI_VISION_FILES) $(CLAD_DEPENDENCIES) $(CLAD_CPP) $(CLAD_CPP_DECL) $(CLAD_CPP_SWITCH)
	if test -f $(OUTPUT_DIR_CTI_VISION)/$*.h; then chmod -f 777 $(OUTPUT_DIR_CTI_VISION)/$*.h; fi
	if test -f $(OUTPUT_DIR_CTI_VISION)/$*.cpp; then chmod -f 777 $(OUTPUT_DIR_CTI_VISION)/$*.cpp; fi
	$(CLAD_CPP) $(CLAD_CPP_FLAGS) -C $(INPUT_CTI_VISION_DIR) -o $(OUTPUT_DIR_CTI_VISION)/ $*.clad
	$(CLAD_CPP_DECL) -C $(INPUT_CTI_VISION_DIR) -o $(OUTPUT_DIR_CTI_VISION)/ $*.clad
	$(CLAD_CPP_SWITCH) -C $(INPUT_CTI_VISION_DIR) -o $(OUTPUT_DIR_CTI_VISION)/ $*.clad
	chmod -f 555 $(OUTPUT_DIR_CTI_VISION)/$*.h
	chmod -f 555 $(OUTPUT_DIR_CTI_VISION)/$*.cpp
	
$(OUTPUT_CTI_COMMON_H_AND_CPP): $(INPUT_CTI_COMMON_FILES) $(CLAD_DEPENDENCIES) $(CLAD_CPP) $(CLAD_CPP_DECL) $(CLAD_CPP_SWITCH)
	if test -f $(OUTPUT_DIR_CTI_COMMON)/$*.h; then chmod -f 777 $(OUTPUT_DIR_CTI_COMMON)/$*.h; fi
	if test -f $(OUTPUT_DIR_CTI_COMMON)/$*.cpp; then chmod -f 777 $(OUTPUT_DIR_CTI_COMMON)/$*.cpp; fi
	$(CLAD_CPP) $(CLAD_CPP_FLAGS) -C $(INPUT_CTI_COMMON_DIR) -o $(OUTPUT_DIR_CTI_COMMON)/ $*.clad
	$(CLAD_CPP_DECL) -C $(INPUT_CTI_COMMON_DIR) -o $(OUTPUT_DIR_CTI_COMMON)/ $*.clad
	$(CLAD_CPP_SWITCH) -C $(INPUT_CTI_COMMON_DIR) -o $(OUTPUT_DIR_CTI_COMMON)/ $*.clad
	chmod -f 555 $(OUTPUT_DIR_CTI_COMMON)/$*.h
	chmod -f 555 $(OUTPUT_DIR_CTI_COMMON)/$*.cpp


# in this target stem does not work. probably issue with patsubst,
# instead of $*.clad use $(@:.cs=.clad)
$(OUTPUT_ROBOT_CSHARP): $(INPUT_ROBOT_SHARED_FILES) $(CLAD_DEPENDENCIES) $(CLAD_CSHARP)
	if test -f $(OUTPUT_DIR_CSHARP)/$@; then chmod -f 777 $(OUTPUT_DIR_CSHARP)/$@; fi
	$(CLAD_CSHARP) -C $(INPUT_ROBOT_DIR) -o $(OUTPUT_DIR_CSHARP)/ $(@:.cs=.clad)
	chmod -f 555 $(OUTPUT_DIR_CSHARP)/$@


# in this target stem does not work. probably issue with patsubst,
# instead of $*.clad use $(@:.cs=.clad)
$(OUTPUT_ENGINE_CSHARP): $(INPUT_ENGINE_FILES) $(CLAD_DEPENDENCIES) $(CLAD_CSHARP) $(ENGINE_GAME_TO_ENGINE_UNION)_hash.cs $(ENGINE_ENGINE_TO_GAME_UNION)_hash.cs	
	if test -f $(OUTPUT_DIR_CSHARP)/$@; then chmod -f 777 $(OUTPUT_DIR_CSHARP)/$@; fi
	$(CLAD_CSHARP) -C $(INPUT_ENGINE_DIR) -I $(INPUT_ROBOT_DIR) $(INPUT_CTI_VISION_DIR) $(INPUT_CTI_COMMON_DIR) $(INPUT_UTIL_DIR) -o $(OUTPUT_DIR_CSHARP)/ $(@:.cs=.clad)
	chmod -f 555 $(OUTPUT_DIR_CSHARP)/$@

# in this target stem does not work. probably issue with patsubst,
# instead of $*.clad use $(@:.cs=.clad)
$(OUTPUT_VIZ_CSHARP): $(INPUT_VIZ_FILES) $(CLAD_DEPENDENCIES) $(CLAD_CSHARP)
	if test -f $(OUTPUT_DIR_CSHARP)/$@; then chmod -f 777 $(OUTPUT_DIR_CSHARP)/$@; fi
	$(CLAD_CSHARP) -C $(INPUT_VIZ_DIR) -I $(INPUT_ROBOT_DIR) $(INPUT_ENGINE_DIR) -o $(OUTPUT_DIR_CSHARP)/ $(@:.cs=.clad)
	chmod -f 555 $(OUTPUT_DIR_CSHARP)/$@
	
# in this target stem does not work. probably issue with patsubst,
# instead of $*.clad use $(@:.cs=.clad)
$(OUTPUT_CTI_VISION_CSHARP): $(INPUT_CTI_VISION_FILES) $(CLAD_DEPENDENCIES) $(CLAD_CSHARP)
	if test -f $(OUTPUT_DIR_CSHARP)/$@; then chmod -f 777 $(OUTPUT_DIR_CSHARP)/$@; fi
	$(CLAD_CSHARP) -C $(INPUT_CTI_VISION_DIR) -o $(OUTPUT_DIR_CSHARP)/ $(@:.cs=.clad)
	chmod -f 555 $(OUTPUT_DIR_CSHARP)/$@
	
# in this target stem does not work. probably issue with patsubst,
# instead of $*.clad use $(@:.cs=.clad)
$(OUTPUT_CTI_COMMON_CSHARP): $(INPUT_CTI_COMMON_FILES) $(CLAD_DEPENDENCIES) $(CLAD_CSHARP)
	if test -f $(OUTPUT_DIR_CSHARP)/$@; then chmod -f 777 $(OUTPUT_DIR_CSHARP)/$@; fi
	$(CLAD_CSHARP) -C $(INPUT_CTI_COMMON_DIR) -o $(OUTPUT_DIR_CSHARP)/ $(@:.cs=.clad)
	chmod -f 555 $(OUTPUT_DIR_CSHARP)/$@

# in this target stem does not work. probably issue with patsubst,
# instead of $*.clad use $(@:.py=.clad)
$(OUTPUT_ROBOT_PYTHON): $(INPUT_ROBOT_SHARED_FILES) $(CLAD_DEPENDENCIES) $(CLAD_PYTHON)
	if test -f $(OUTPUT_DIR_PYTHON)/$@; then chmod -f 777 $(OUTPUT_DIR_PYTHON)/$@; fi
	$(CLAD_PYTHON) -C $(INPUT_ROBOT_DIR) -o $(OUTPUT_DIR_PYTHON)/ $(@:.py=.clad)
	chmod -f 555 $(OUTPUT_DIR_PYTHON)/$@

# in this target stem does not work. probably issue with patsubst,
# instead of $*.clad use $(@:.py=.clad)
$(OUTPUT_ENGINE_PYTHON): $(INPUT_ENGINE_FILES) $(CLAD_DEPENDENCIES) $(CLAD_PYTHON) $(ENGINE_GAME_TO_ENGINE_UNION)_hash.py $(ENGINE_ENGINE_TO_GAME_UNION)_hash.py	
	if test -f $(OUTPUT_DIR_PYTHON)/$@; then chmod -f 777 $(OUTPUT_DIR_PYTHON)/$@; fi
	$(CLAD_PYTHON) -C $(INPUT_ENGINE_DIR) -I $(INPUT_ROBOT_DIR) $(INPUT_CTI_VISION_DIR) $(INPUT_CTI_COMMON_DIR) $(INPUT_UTIL_DIR) -o $(OUTPUT_DIR_PYTHON)/ $(@:.py=.clad)
	chmod -f 555 $(OUTPUT_DIR_PYTHON)/$@

$(OUTPUT_UTIL_PYTHON): $(INPUT_UTIL_FILES) $(CLAD_DEPENDENCIES) $(CLAD_PYTHON)
	if test -f $(OUTPUT_DIR_PYTHON)/$@; then chmod -f 777 $(OUTPUT_DIR_PYTHON)/$@; fi
	$(CLAD_PYTHON) -C $(INPUT_UTIL_DIR) -I $(INPUT_UTIL_DIR) -o $(OUTPUT_DIR_PYTHON)/ $(@:.py=.clad)
	chmod -f 555 $(OUTPUT_DIR_PYTHON)/$@

# in this target stem does not work. probably issue with patsubst,
# instead of $*.clad use $(@:.py=.clad)
$(OUTPUT_VIZ_PYTHON): $(INPUT_VIZ_FILES) $(CLAD_DEPENDENCIES) $(CLAD_PYTHON)
	if test -f $(OUTPUT_DIR_PYTHON)/$@; then chmod -f 777 $(OUTPUT_DIR_PYTHON)/$@; fi
	$(CLAD_PYTHON) -C $(INPUT_VIZ_DIR) -I $(INPUT_ROBOT_DIR) $(INPUT_ENGINE_DIR) -o $(OUTPUT_DIR_PYTHON)/ $(@:.py=.clad)
	chmod -f 555 $(OUTPUT_DIR_PYTHON)/$@

# in this target stem does not work. probably issue with patsubst,
# instead of $*.clad use $(@:.py=.clad)
$(OUTPUT_CTI_VISION_PYTHON): $(INPUT_CTI_VISION_FILES) $(CLAD_DEPENDENCIES) $(CLAD_PYTHON)
	if test -f $(OUTPUT_DIR_PYTHON)/$@; then chmod -f 777 $(OUTPUT_DIR_PYTHON)/$@; fi
	$(CLAD_PYTHON) -C $(INPUT_CTI_VISION_DIR) -o $(OUTPUT_DIR_PYTHON)/ $(@:.py=.clad)
	chmod -f 555 $(OUTPUT_DIR_PYTHON)/$@
	
# in this target stem does not work. probably issue with patsubst,
# instead of $*.clad use $(@:.py=.clad)
$(OUTPUT_CTI_COMMON_PYTHON): $(INPUT_CTI_COMMON_FILES) $(CLAD_DEPENDENCIES) $(CLAD_PYTHON)
	if test -f $(OUTPUT_DIR_PYTHON)/$@; then chmod -f 777 $(OUTPUT_DIR_PYTHON)/$@; fi
	$(CLAD_PYTHON) -C $(INPUT_CTI_COMMON_DIR) -o $(OUTPUT_DIR_PYTHON)/ $(@:.py=.clad)
	chmod -f 555 $(OUTPUT_DIR_PYTHON)/$@
	
# clad specific paths in here..
$(OUTPUT_VIZ_LST): 
	#$(INPUT_VIZ_FILES)
	cat /dev/null > $(OUTPUT_VIZ_LST)
	for f in $(OUTPUT_VIZ_H_AND_CPP); do echo ../$(OUTPUT_DIR_ENGINE)/$$f >> $(OUTPUT_VIZ_LST); done

$(OUTPUT_ENGINE_LST):
	cat /dev/null > $(OUTPUT_ENGINE_LST)
	for f in $(OUTPUT_ENGINE_H_AND_CPP); do echo ../$(OUTPUT_DIR_ENGINE)/$$f >> $(OUTPUT_ENGINE_LST); done

$(OUTPUT_ROBOT_LST):
	cat /dev/null > $(OUTPUT_ROBOT_LST)
	for f in $(OUTPUT_ROBOT_H_AND_CPP); do echo ../$(OUTPUT_DIR_ENGINE)/$$f >> $(OUTPUT_ROBOT_LST); done

$(OUTPUT_CTI_VISION_LST):
	cat /dev/null > $(OUTPUT_CTI_VISION_LST)
	for f in $(OUTPUT_CTI_VISION_H_AND_CPP); do echo ../../generated/clad/vision/$$f >> $(OUTPUT_CTI_VISION_LST); done

$(OUTPUT_CTI_VISION_ENGINE_LST):
	cat /dev/null > $(OUTPUT_CTI_VISION_ENGINE_LST)
	for f in $(OUTPUT_CTI_VISION_H_AND_CPP); do echo ../../coretech/generated/clad/vision/$$f >> $(OUTPUT_CTI_VISION_ENGINE_LST); done

$(OUTPUT_CTI_COMMON_LST):
	cat /dev/null > $(OUTPUT_CTI_COMMON_LST)
	for f in $(OUTPUT_CTI_COMMON_H_AND_CPP); do echo ../../generated/clad/common/$$f >> $(OUTPUT_CTI_COMMON_LST); done

$(OUTPUT_CTI_COMMON_ENGINE_LST):
	cat /dev/null > $(OUTPUT_CTI_COMMON_ENGINE_LST)
	for f in $(OUTPUT_CTI_COMMON_H_AND_CPP); do echo ../../coretech/generated/clad/common/$$f >> $(OUTPUT_CTI_COMMON_ENGINE_LST); done

show:
	echo "*** input engine files ***"
	echo $(INPUT_ENGINE_FILES)
	echo "*** output files engine cpp ***"
	echo $(OUTPUT_ENGINE_H_AND_CPP)
	echo "*** input files robot ***"
	echo $(INPUT_ROBOT_SHARED_FILES)
	echo "*** input files ctiVision ***"
	echo $(INPUT_CTI_VISION_FILES)
	echo "*** input files ctiCommon ***"
	echo $(INPUT_CTI_COMMON_FILES)
	echo "*** output files robot cpp ***"
	echo $(OUTPUT_ROBOT_H_AND_CPP)
	echo "*** output files ctiVision ***"
	echo $(OUTPUT_CTI_VISION_H_AND_CPP)
	echo "*** output files ctiCommon ***"
	echo $(OUTPUT_CTI_COMMON_H_AND_CPP)
	echo "*** output engine C# ***"
	echo $(OUTPUT_ENGINE_CSHARP)
	echo "*** output robot C# ***"
	echo $(OUTPUT_ROBOT_CSHARP)
	echo "*** output ctiVision C# ***"
	echo $(OUTPUT_CTI_VISION_CSHARP)
	echo "*** output ctiCommon C# ***"
	echo $(OUTPUT_CTI_COMMON_CSHARP)
	echo "*** output viz C# ***"
	echo $(OUTPUT_VIZ_CSHARP)
	echo "*** output engine python ***"
	echo $(OUTPUT_ENGINE_PYTHON)
	echo "*** output robot python ***"
	echo $(OUTPUT_ROBOT_PYTHON)
	echo "*** output ctiVision python ***"
	echo $(OUTPUT_CTI_VISION_PYTHON)
	echo "*** output ctiCommon python ***"
	echo $(OUTPUT_CTI_COMMON_PYTHON)
	echo "*** input util files ***"
	echo $(INPUT_UTIL_FILES)
	echo "*** output files util python ***"
	echo $(OUTPUT_UTIL_PYTHON)

clean: cleanOutputDirs cleanCppListFiles

cleanCppListFiles:
	rm -rf $(OUTPUT_VIZ_LST) $(OUTPUT_ENGINE_LST) $(OUTPUT_ROBOT_LST) $(OUTPUT_CTI_VISION_LST) $(OUTPUT_CTI_VISION_ENGINE_LST) $(OUTPUT_CTI_COMMON_LST) $(OUTPUT_CTI_COMMON_ENGINE_LST)

cleanOutputDirs:
	if test -d $(OUTPUT_DIR_ENGINE); then chmod -Rf 777 $(OUTPUT_DIR_ENGINE); fi
	if test -d $(OUTPUT_DIR_CSHARP); then chmod -Rf 777 $(OUTPUT_DIR_CSHARP); fi
	if test -d $(OUTPUT_DIR_PYTHON); then chmod -Rf 777 $(OUTPUT_DIR_PYTHON); fi
	if test -d $(OUTPUT_DIR_CTI_VISION); then chmod -Rf 777 $(OUTPUT_DIR_CTI_VISION); fi
	if test -d $(OUTPUT_DIR_CTI_COMMON); then chmod -Rf 777 $(OUTPUT_DIR_CTI_COMMON); fi
	rm -rf $(OUTPUT_DIR_ENGINE) $(OUTPUT_DIR_CSHARP) $(OUTPUT_DIR_PYTHON) $(OUTPUT_DIR_CTI_VISION) $(OUTPUT_DIR_CTI_COMMON)
