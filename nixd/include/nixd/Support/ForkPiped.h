#pragma once

namespace nixd {

/// \brief fork this process and create some pipes connected to the new process.
///
/// Connect the child's standard streams to parent-owned descriptors: In for
/// writing input, Out and Err for reading output. Executed programs inherit the
/// child's standard streams, but not the parent endpoints.
///
/// The child must execute a new program or terminate without normal cleanup,
/// and must not call this helper again. Use this helper consistently for
/// concurrent process creation to ensure descriptor isolation across platforms.
///
/// \returns pid of child process, in parent.
/// \returns 0 in child.
int forkPiped(int &In, int &Out, int &Err);

} // namespace nixd
