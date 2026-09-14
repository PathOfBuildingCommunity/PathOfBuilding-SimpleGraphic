// SimpleGraphic Engine
//
// macOS platform helpers
//

#pragma once

struct GLFWwindow;

// Keep the layer being rendered into at the same pixel density as the display
// the window is on. Returns true when it had to be corrected.
bool Platform_SyncLayerScale(GLFWwindow* window);
