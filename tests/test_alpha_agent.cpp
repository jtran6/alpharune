/// Seats the `alpha` agent through the riftbound binary — the same spec the
/// CLI accepts — and plays one short game to completion.
///
/// A finished game means the kind is selectable and every choice was one the
/// engine offered (anything else throws, and the process exits non-zero).
/// The short game uses sims=1 so it stays small. The bare name `alpha` is the
/// default seat, AlphaAgent::kDefaultSims (50). Finishing is not a measure of
/// how well the agent plays.

#include <gtest/gtest.h>

#include "agents/mcts_agent.h"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern "C" char** environ;

namespace riftbound::test::alpha_agent {

namespace fs = std::filesystem;

struct Captured {
    int code = -1;
    std::string output;
    std::string error;  // spawn failure; empty when the process ran
};

std::string riftboundBinary() {
    std::error_code ec;
    const fs::path exe = fs::canonical("/proc/self/exe", ec);
    if (ec) return {};
    return (exe.parent_path() / "riftbound").string();
}

std::string deckPath(const char* name) {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    const fs::path base = root && *root ? fs::path(root) : fs::path("..");
    return fs::absolute(base / "decks" / name).string();
}

Captured runBinary(const std::string& bin, const std::vector<std::string>& args) {
    Captured out;
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        out.error = std::string("pipe: ") + std::strerror(errno);
        return out;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipefd[0]);
    posix_spawn_file_actions_addclose(&actions, pipefd[1]);

    std::vector<std::string> owned;
    owned.reserve(args.size() + 1);
    owned.push_back(bin);
    owned.insert(owned.end(), args.begin(), args.end());
    std::vector<char*> argv;
    argv.reserve(owned.size() + 1);
    for (std::string& s : owned) argv.push_back(s.data());
    argv.push_back(nullptr);

    pid_t pid = 0;
    const int spawned = posix_spawn(&pid, bin.c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipefd[1]);
    if (spawned != 0) {
        close(pipefd[0]);
        out.error = std::string("posix_spawn: ") + std::strerror(spawned);
        return out;
    }

    char buf[4096];
    ssize_t n = 0;
    while ((n = read(pipefd[0], buf, sizeof buf)) > 0) out.output.append(buf, static_cast<size_t>(n));
    close(pipefd[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        out.error = std::string("waitpid: ") + std::strerror(errno);
        return out;
    }
    out.code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return out;
}

std::string tail(const std::string& text, std::size_t n = 1500) {
    if (text.size() <= n) return text;
    return text.substr(text.size() - n);
}

// The kind has to be on the CLI. A missing binary, an unknown kind, or a
// bare `alpha` that the parser rejects fails this test. The game is short
// (sims=1); the default seat, with no sims written, is 50.
TEST(AlphaAgent, IsSelectableAndPlaysALegalGame) {
    EXPECT_EQ(AlphaAgent::kDefaultSims, 50);

    const std::string bin = riftboundBinary();
    ASSERT_FALSE(bin.empty());
    ASSERT_TRUE(fs::is_regular_file(bin)) << bin;

    const Captured help = runBinary(bin, {"--help"});
    ASSERT_TRUE(help.error.empty()) << help.error;
    ASSERT_EQ(help.code, 0) << tail(help.output);
    EXPECT_NE(help.output.find("alpha[:sims=N]"), std::string::npos) << tail(help.output);
    EXPECT_NE(help.output.find("N is 50"), std::string::npos) << tail(help.output);

    const std::vector<std::string> decks = {
        "--deck1", "/tmp/alpharune-no-such-deck-a.txt",
        "--deck2", "/tmp/alpharune-no-such-deck-b.txt",
        "--web", "off",
        "--render-html", "off",
    };

    std::vector<std::string> unknown = decks;
    unknown.insert(unknown.end(), {"--agent1", "not-an-agent", "--agent2", "random"});
    const Captured rejected = runBinary(bin, unknown);
    ASSERT_TRUE(rejected.error.empty()) << rejected.error;
    EXPECT_NE(rejected.code, 0);
    EXPECT_NE(rejected.output.find("Unknown agent kind"), std::string::npos) << tail(rejected.output);

    std::vector<std::string> bare = decks;
    bare.insert(bare.end(), {"--agent1", "alpha", "--agent2", "random"});
    const Captured accepted = runBinary(bin, bare);
    ASSERT_TRUE(accepted.error.empty()) << accepted.error;
    EXPECT_NE(accepted.code, 0) << tail(accepted.output);
    EXPECT_EQ(accepted.output.find("Unknown agent kind"), std::string::npos) << tail(accepted.output);
    EXPECT_EQ(accepted.output.find("requires sims"), std::string::npos) << tail(accepted.output);
    EXPECT_NE(accepted.output.find("Failed to load deck"), std::string::npos) << tail(accepted.output);

    const std::string deck1 = deckPath("yi_rebuild.txt");
    const std::string deck2 = deckPath("fiora.txt");
    ASSERT_TRUE(fs::is_regular_file(deck1)) << deck1;
    ASSERT_TRUE(fs::is_regular_file(deck2)) << deck2;

    // sims=1 keeps the game short. The seat is still AlphaAgent, and the
    // engine rejects an answer that was not offered.
    const Captured game = runBinary(bin, {
        "--deck1", deck1,
        "--deck2", deck2,
        "--agent1", "alpha:sims=1",
        "--agent2", "random",
        "--seed", "11",
        "--web", "off",
        "--render-html", "off",
    });
    ASSERT_TRUE(game.error.empty()) << game.error;
    ASSERT_EQ(game.code, 0) << tail(game.output);
    EXPECT_NE(game.output.find("agents alpha:sims=1 vs random."), std::string::npos)
        << tail(game.output);
    const bool finished = game.output.find(" wins. ") != std::string::npos ||
                          game.output.find("No winner. ") != std::string::npos;
    EXPECT_TRUE(finished) << tail(game.output);
    EXPECT_EQ(game.output.find("Not usable"), std::string::npos) << tail(game.output);
    EXPECT_EQ(game.output.find("ENGINE_EXCEPTION"), std::string::npos) << tail(game.output);
    EXPECT_EQ(game.output.find("Unknown agent kind"), std::string::npos) << tail(game.output);
    EXPECT_NE(game.output.find("Decisions "), std::string::npos) << tail(game.output);
}

}  // namespace riftbound::test::alpha_agent
