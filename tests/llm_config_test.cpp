// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "test_util.h"

#include <cstdlib>

#include "ai/llm.h"

using ocs::llm::Config;
using ocs::llm::config_from_env;
using ocs::llm::set_config;
using ocs::llm::config;

namespace {
struct EnvGuard {
    const char* name;
    EnvGuard(const char* n) : name(n) {}
    ~EnvGuard() { unsetenv(name); }
};
}  // namespace

TEST(llm_config_from_env_defaults) {
    EnvGuard o("OCS_OLLAMA_URL"), l("OCS_LLAMA_URL"), c("OCS_AI_LAUNCH_CMD"),
        d("OCS_AI_LAUNCH_DIR"), a("OCS_AI_AUTO_LAUNCH");
    const Config cfg = config_from_env();
    CHECK(cfg.ollama_url == "http://localhost:11434");
    CHECK(cfg.llama_url == "http://localhost:8080");
    CHECK(cfg.launch_command.empty());
    CHECK(cfg.launch_dir.empty());
    CHECK(cfg.auto_launch == false);
}

TEST(llm_config_from_env_overrides) {
    EnvGuard o("OCS_OLLAMA_URL"), l("OCS_LLAMA_URL"), c("OCS_AI_LAUNCH_CMD"),
        d("OCS_AI_LAUNCH_DIR"), a("OCS_AI_AUTO_LAUNCH");
    setenv("OCS_OLLAMA_URL", "http://10.0.0.5:11434", 1);
    setenv("OCS_LLAMA_URL", "http://10.0.0.5:8080", 1);
    setenv("OCS_AI_LAUNCH_CMD", "ollama serve", 1);
    setenv("OCS_AI_LAUNCH_DIR", "/opt/ai", 1);
    setenv("OCS_AI_AUTO_LAUNCH", "1", 1);
    const Config cfg = config_from_env();
    CHECK(cfg.ollama_url == "http://10.0.0.5:11434");
    CHECK(cfg.llama_url == "http://10.0.0.5:8080");
    CHECK(cfg.launch_command == "ollama serve");
    CHECK(cfg.launch_dir == "/opt/ai");
    CHECK(cfg.auto_launch == true);
}

TEST(llm_config_set_normalizes_urls) {
    Config c;
    c.ollama_url = "http://host:1234/";
    c.llama_url = "http://host:5678///";
    set_config(c);
    CHECK(config().ollama_url == "http://host:1234");
    CHECK(config().llama_url == "http://host:5678");
}

TEST(llm_config_auto_launch_flag_parsing) {
    EnvGuard a("OCS_AI_AUTO_LAUNCH");
    setenv("OCS_AI_AUTO_LAUNCH", "true", 1);
    CHECK(config_from_env().auto_launch == true);
    setenv("OCS_AI_AUTO_LAUNCH", "yes", 1);
    CHECK(config_from_env().auto_launch == true);
    setenv("OCS_AI_AUTO_LAUNCH", "0", 1);
    CHECK(config_from_env().auto_launch == false);
    setenv("OCS_AI_AUTO_LAUNCH", "garbage", 1);
    CHECK(config_from_env().auto_launch == false);
}
