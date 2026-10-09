#include "project/project_session.h"

// STUB (test-writer, M1-04): every operation fails with "not_implemented", the implementer replaces this file.
namespace tracklab::project
{

struct ProjectSession::Impl
{
    core::EditContext* context = nullptr;
};

namespace
{
[[noreturn]] void notImplemented()
{
    throw core::CommandFailure("not_implemented", "ProjectSession is not implemented yet");
}
}  // namespace

ProjectSession::ProjectSession(te::Engine&, core::EditContext& context) : impl(std::make_unique<Impl>())
{
    impl->context = &context;
}

ProjectSession::~ProjectSession() = default;

te::Edit* ProjectSession::edit() const noexcept
{
    return nullptr;
}

void ProjectSession::setBeforeReplaceHook(BeforeReplaceHook) {}

ProjectInfo ProjectSession::createProject(const juce::File&, const juce::String&)
{
    notImplemented();
}

ProjectInfo ProjectSession::openProject(const juce::File&)
{
    notImplemented();
}

ProjectInfo ProjectSession::save()
{
    notImplemented();
}

ProjectInfo ProjectSession::saveAs(const juce::File&, const juce::String&)
{
    notImplemented();
}

void ProjectSession::closeProject(bool)
{
    notImplemented();
}

ProjectInfo ProjectSession::info() const
{
    notImplemented();
}

}  // namespace tracklab::project
