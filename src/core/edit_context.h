// Edit context (M1-03): how commands reach the project that is currently open, without global state.
// The app (or a test, or the CLI) owns one EditContext, points it at the open Edit and hands it to
// CommandRegistry::setEditContext() and to every registerXxxCommands() whose handlers need the project.
#pragma once

namespace tracktion::inline engine
{
class Edit;
}

namespace tracklab::core
{

/** Non-owning, message-thread-only reference to the current Edit; null = no project open.
    The Edit has to outlive the context's use of it: call setEdit(nullptr) before the Edit is destroyed. */
class EditContext
{
public:
    EditContext() = default;
    explicit EditContext(tracktion::Edit* current) noexcept : currentEdit(current) {}

    void setEdit(tracktion::Edit* current) noexcept { currentEdit = current; }
    tracktion::Edit* edit() const noexcept { return currentEdit; }

private:
    tracktion::Edit* currentEdit = nullptr;
};

}  // namespace tracklab::core
