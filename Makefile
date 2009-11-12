PKG_CONFIG_PACKAGES=glib-2.0
LDFLAGS=-lm `pkg-config ${PKG_CONFIG_PACKAGES} --libs`
CFLAGS=-Wall -g `pkg-config ${PKG_CONFIG_PACKAGES} --cflags`

PRED_OBJECTS = \
	pred_src/pred.o                         \
	pred_src/run_model.o                    \
	pred_src/altitude.o                     \
	pred_src/ini/dictionary.o               \
	pred_src/ini/iniparser.o                \
	pred_src/util/getline.o                 \
	pred_src/util/getdelim.o                \
	pred_src/util/gopt.o                    \
	pred_src/util/random.o                  \
	pred_src/wind/wind_file_cache.o         \
	pred_src/wind/wind_file.o 

PRED_EXECUTABLE=pred

all: $(PRED_EXECUTABLE) test

$(PRED_EXECUTABLE): $(PRED_OBJECTS)
	$(CC) $(LDFLAGS) $(PRED_OBJECTS) -o $@

clean_pred:
	rm -rf $(PRED_OBJECTS) $(PRED_EXECUTABLE)

clean: clean_pred

test: $(PRED_EXECUTABLE) test/scenario.ini
	./$(PRED_EXECUTABLE) -v -i test/gfs -t 1257951600 test/scenario.ini > test/output.csv
