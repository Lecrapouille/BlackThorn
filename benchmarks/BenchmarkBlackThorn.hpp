/**
 * @file BenchmarkBlackThorn.hpp
 * @brief BlackThorn benchmark helpers.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BenchmarkCommon.hpp"
#include "BlackThorn/BlackThorn.hpp"

namespace bt::benchmark {

inline std::filesystem::path yamlPath(std::string const& p_filename)
{
    return ::benchmark::benchmarksRoot() / "yaml" / p_filename;
}

// ---------------------------------------------------------------------------
//! \brief Action that resolves three remapped ports on every tick.
// ---------------------------------------------------------------------------
class ReadPorts final: public CallbackLeaf
{
public:

    ReadPorts() : CallbackLeaf([]() { return Status::SUCCESS; }) {}

    [[nodiscard]] static constexpr char const* toString()
    {
        return "Action";
    }

    void fillConfig(NodeConfig& p_config) const override
    {
        p_config.callback = [this]() {
            (void)getInput<int>("a");
            (void)getInput<int>("b");
            (void)getInput<std::string>("label");
            return Status::SUCCESS;
        };
    }
};

inline void registerBenchmarkNodes(NodeFactory& p_factory)
{
    auto success = []() { return Status::SUCCESS; };

    for (char const* name :
         {"LoadRoute", "FollowWaypoints", "AttemptNonLethal",
          "NeutralizeThreat", "ExtractTeam", "LoadGameState",
          "ChoosePrimaryEnemy"})
    {
        p_factory.registerAction(name, success);
    }

    p_factory.registerNode<ReadPorts>("ReadPorts");
}

inline robotik::Return<Tree::Ptr>
loadTreeFromYaml(NodeFactory const& p_factory,
                 std::filesystem::path const& p_path,
                 Blackboard::Ptr p_blackboard = nullptr)
{
    return Builder::fromFile(p_factory, p_path.string(), p_blackboard);
}

} // namespace bt::benchmark
