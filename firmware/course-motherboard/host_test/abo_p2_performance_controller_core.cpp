// Keep the production translation unit inside the host_test source tree.
// The project path contains non-ASCII characters and '#'; MinGW otherwise
// mirrors the external source path into an object directory it cannot create.
#include "../abo_p2/performance_clock.cpp"
#include "../abo_p2/performance_controller.cpp"
