# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS 
    	"main.c"
    	"../peripheral/lcd.c"
    	"../peripheral/ad8232.c"
    INCLUDE_DIRS
    	"."        
    	"../peripheral"
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES            # optional, list the public requirements (component names)
    PRIV_REQUIRES       # optional, list the private requirements
)
