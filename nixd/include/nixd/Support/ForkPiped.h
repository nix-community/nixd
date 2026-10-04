#pragma once

namespace nixd {

/// \brief fork this process and create some pipes connected to the new process.
///
/// \returns pid of child process, in parent.
/// \returns 0 in child.
int forkPiped(int &In, int &Out, int &Err);

} // namespace nixd
