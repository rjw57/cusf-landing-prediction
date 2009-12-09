# CMake build file for the CUSF landing prediction software
#
# Original version: Rich Wareham <rjw57@cam.ac.uk>

add_custom_command(
	OUTPUT
		output.csv
	COMMAND 
		../pred_src/pred -v -i gfs scenario-1.ini scenario-2.ini > output.csv
	COMMAND 
		../pred_src/pred -v -i gfs < scenario-1.ini
	DEPENDS
		pred
)

add_custom_target(test ALL DEPENDS output.csv)

