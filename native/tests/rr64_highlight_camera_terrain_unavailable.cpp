// Ordering/readiness fixtures have no prepared immutable terrain bank. Keep
// that explicit boundary; actual floor/native/scope behavior has its own test.
#include "rr64_highlight_camera_terrain.hpp"
namespace rr64::highlight_camera::terrain {
bool clear_eye(unsigned char*, View&) noexcept { return false; }
void reset() noexcept {}
}
