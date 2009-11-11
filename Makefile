
LDFLAGS=-lm
CFLAGS=-Wall

PRED_OBJECTS = \
	pred_src/pred.o \
	pred_src/gopt.o \
	pred_src/run_model.o \
	pred_src/altitude.o \
	pred_src/wind_files.o \
	pred_src/wind_file.o \
	pred_src/getline.o \
	pred_src/getdelim.o \
	pred_src/ini/dictionary.o \
	pred_src/ini/iniparser.o 

PRED_EXECUTABLE=pred

all: $(PRED_EXECUTABLE)

$(PRED_EXECUTABLE): $(PRED_OBJECTS)
	$(CC) $(LDFLAGS) $(PRED_OBJECTS) -o $@

clean_pred:
	rm -rf $(PRED_OBJECTS) $(PRED_EXECUTABLE)

clean: clean_pred
