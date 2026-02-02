CXX := g++

SRCDIR := src
OBJDIR := obj
INCDIR := inc
INCPATHS := -I $(INCDIR) -I $(INCDIR)/parser -I $(INCDIR)/sta

OUTPUTDIR := result
TESTDIR := testcase

# find all .cpp files under src recursively
SRCS_TOP := $(shell find $(SRCDIR) -type f -name '*.cpp')
# include main.cpp (located in repo root)
SRCS := $(SRCS_TOP) main.cpp

# preserve directory structure under obj/
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS_TOP)) $(OBJDIR)/main.o
DEPS := $(OBJS:.o=.d)

CXXFLAGS := $(INCPATHS) -std=c++11 -MMD -MP
LINKFLAGS := -pedantic -fomit-frame-pointer -flto -ffast-math -O3 -g

TARGET := sta

.PHONY: all clean

all: $(TARGET)

# link
$(TARGET): $(OBJS)
	$(CXX) $(LINKFLAGS) $(OBJS) -o $@

# pattern rule for src/... -> obj/...
# creates target directory as needed
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(LINKFLAGS) -c $< -o $@

# rule to build main.cpp -> obj/main.o
$(OBJDIR)/main.o: main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(LINKFLAGS) -c $< -o $@

# include dependency files
-include $(DEPS)

$(OUTPUTDIR):
	@mkdir $(OUTPUTDIR)

clean:
	rm -rf $(TARGET) $(OBJDIR) *.txt 314510120_PA2 314510120_PA2.tar

#=======================================================
#                Test Area
#=======================================================
test: | $(OUTPUTDIR)
	@./sta $(TESTDIR)/c17/c17.v -l $(TESTDIR)/test_lib.lib -i $(TESTDIR)/c17/c17.pat

test2: | $(OUTPUTDIR)
	@./sta $(TESTDIR)/c432/c432.v -l $(TESTDIR)/test_lib.lib -i $(TESTDIR)/c432/c432.pat

test3: | $(OUTPUTDIR)
	@./sta $(TESTDIR)/c123456/c123456.v -l $(TESTDIR)/test_lib.lib -i $(TESTDIR)/c123456/c123456.pat

test1: | $(OUTPUTDIR)
	@./sta $(TESTDIR)/c54321/c54321.v -l $(TESTDIR)/test_lib.lib -i $(TESTDIR)/c54321/c54321.pat

test4: | $(OUTPUTDIR)
	@./sta $(TESTDIR)/hellish/hellish.v -l $(TESTDIR)/test_lib.lib -i $(TESTDIR)/hellish/hellish.pat

test_swap_pin: | $(OUTPUTDIR)
	@./sta $(TESTDIR)/c17_swap_pin/c17.v -l $(TESTDIR)/test_lib.lib -i $(TESTDIR)/c17_swap_pin/c17.pat

test_lib: | $(OUTPUTDIR)
	@./sta $(TESTDIR)/c17/c17.v -l $(TESTDIR)/test2_lib.lib -i $(TESTDIR)/c17/c17.pat

314510120_PA2:
	@mkdir 314510120_PA2

tar: | 314510120_PA2
	@cp -r $(SRCDIR) 314510120_PA2
	@cp -r $(INCDIR) 314510120_PA2
	@cp Makefile 314510120_PA2
	@cp main.cpp 314510120_PA2
	@tar -cvf 314510120_PA2.tar 314510120_PA2
