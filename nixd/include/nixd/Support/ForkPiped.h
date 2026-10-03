#pragma once

namespace nixd {

/// \brief fork this process and create some pipes connected to the new process.
///
/// The child's stdin, stdout and stderr are connected to pipes. In the parent,
/// In is the writable stdin end, and Out and Err are the readable output ends.
/// The returned descriptors close on exec; the child's stdio survives exec.
///
/// The child must exec or _exit and must not call forkPiped again. On platforms
/// without pipe2, concurrent process creation must use this helper to avoid
/// inheriting descriptors before their close-on-exec flags have been set.
///
/// \returns pid of child process, in parent.
/// \returns 0 in child.
int forkPiped(int &In, int &Out, int &Err);

} // namespace nixd
