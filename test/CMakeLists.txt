# CMake build file for the CUSF landing prediction software
#
# Original version: Rich Wareham <rjw57@cam.ac.uk>

add_custom_command(
	OUTPUT
		output.csv
	COMMAND 
		pred -v -i gfs -t 1257951600 scenario.ini > output.csv
	DEPENDS
		pred
)

add_custom_target(test ALL DEPENDS output.csv)

