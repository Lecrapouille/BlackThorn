/**
 * @file main.cpp
 * @brief Comparative micro-benchmarks: BlackThorn vs BehaviorTree.CPP.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "BenchmarkBT.hpp"
#include "BenchmarkBlackThorn.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

constexpr std::size_t kWarmupTicks = 100;
constexpr std::size_t kTickIterations = 50000;
constexpr std::size_t kResetIterations = 1000;
constexpr std::size_t kLoadIterations = 200;

void warmupBlackThorn(bt::Tree& p_tree)
{
    for (std::size_t i = 0; i < kWarmupTicks; ++i)
    {
        static_cast<void>(p_tree.tick());
        p_tree.reset();
    }
}

void warmupBT(BT::Tree& p_tree)
{
    for (std::size_t i = 0; i < kWarmupTicks; ++i)
    {
        p_tree.tickOnce();
        p_tree.haltTree();
    }
}

void benchBlackThornYamlLoad(char const* p_name,
                             std::filesystem::path const& p_path,
                             bt::NodeFactory const& p_factory,
                             bt::Blackboard::Ptr p_blackboard = nullptr)
{
    double const total_ms = ::benchmark::runTimed(kLoadIterations, [&]() {
        auto result =
            bt::benchmark::loadTreeFromYaml(p_factory, p_path, p_blackboard);
        if (!result.isSuccess())
        {
            throw std::runtime_error(result.getError());
        }
    });
    ::benchmark::printResult(p_name, kLoadIterations, total_ms);
}

void benchBlackThornYamlParse(char const* p_name,
                              std::filesystem::path const& p_path)
{
    double const total_ms = ::benchmark::runTimed(kLoadIterations, [&]() {
        auto result = bt::benchmark::parseTreeDocument(p_path);
        if (!result.isSuccess())
        {
            throw std::runtime_error(result.getError());
        }
    });
    ::benchmark::printResult(p_name, kLoadIterations, total_ms);
}

void benchBlackThornYamlBuild(char const* p_name,
                              std::filesystem::path const& p_path,
                              bt::NodeFactory const& p_factory,
                              bt::Blackboard::Ptr p_blackboard = nullptr)
{
    auto document = bt::benchmark::parseTreeDocument(p_path);
    if (!document.isSuccess())
    {
        throw std::runtime_error(document.getError());
    }

    double const total_ms = ::benchmark::runTimed(kLoadIterations, [&]() {
        auto result = bt::benchmark::instantiateTree(
            p_factory, document.getValue(), p_blackboard);
        if (!result.isSuccess())
        {
            throw std::runtime_error(result.getError());
        }
    });
    ::benchmark::printResult(p_name, kLoadIterations, total_ms);
}

void benchBlackThornTick(char const* p_name, bt::Tree& p_tree)
{
    warmupBlackThorn(p_tree);
    double const total_ms = ::benchmark::runTimed(kTickIterations, [&]() {
        static_cast<void>(p_tree.tick());
        p_tree.reset();
    });
    ::benchmark::printResult(p_name, kTickIterations, total_ms);
}

void benchBTXmlLoad(char const* p_name,
                    std::filesystem::path const& p_path,
                    BT::BehaviorTreeFactory& p_factory)
{
    double const total_ms = ::benchmark::runTimed(kLoadIterations, [&]() {
        auto tree = btcpp::benchmark::loadTreeFromXml(p_factory, p_path);
        (void)tree;
    });
    ::benchmark::printResult(p_name, kLoadIterations, total_ms);
}

void benchBTTick(char const* p_name, BT::Tree& p_tree)
{
    warmupBT(p_tree);
    double const total_ms = ::benchmark::runTimed(kTickIterations, [&]() {
        p_tree.tickOnce();
        p_tree.haltTree();
    });
    ::benchmark::printResult(p_name, kTickIterations, total_ms);
}

void runBlackThornBenchmarks()
{
    using namespace bt;

    std::cout << "=== BlackThorn ===\n";
    std::cout << "YAML: " << ::benchmark::benchmarksRoot() / "yaml" << "\n\n";

    NodeFactory factory;
    bt::benchmark::registerBenchmarkNodes(factory);

    auto const parallel_yaml =
        bt::benchmark::yamlPath("parallel_100_success.yaml");
    auto const sequence_yaml =
        bt::benchmark::yamlPath("sequence_depth_50.yaml");
    auto const patrol_yaml = bt::benchmark::yamlPath("patrol.yaml");
    auto const game_state_yaml = bt::benchmark::yamlPath("game_state.yaml");
    auto const bb_writes_yaml =
        bt::benchmark::yamlPath("blackboard_writes.yaml");
    auto const bb_reads_yaml =
        bt::benchmark::yamlPath("blackboard_port_reads.yaml");
    auto const bb_remap_yaml =
        bt::benchmark::yamlPath("blackboard_subtree_remap.yaml");
    auto const bb_large_yaml = bt::benchmark::yamlPath("blackboard_large.yaml");

    std::cout << "--- XML/YAML load (Builder::fromFile) ---\n";
    benchBlackThornYamlParse("[BlackThorn] load.parse.parallel_100_success",
                             parallel_yaml);
    benchBlackThornYamlBuild(
        "[BlackThorn] load.build.parallel_100_success", parallel_yaml, factory);
    benchBlackThornYamlLoad(
        "[BlackThorn] load.full.parallel_100_success", parallel_yaml, factory);

    benchBlackThornYamlParse("[BlackThorn] load.parse.sequence_depth_50",
                             sequence_yaml);
    benchBlackThornYamlBuild(
        "[BlackThorn] load.build.sequence_depth_50", sequence_yaml, factory);
    benchBlackThornYamlLoad(
        "[BlackThorn] load.full.sequence_depth_50", sequence_yaml, factory);

    auto patrol_bb = std::make_shared<Blackboard>();
    benchBlackThornYamlLoad(
        "[BlackThorn] load.patrol", patrol_yaml, factory, patrol_bb);

    auto game_bb = std::make_shared<Blackboard>();
    benchBlackThornYamlLoad(
        "[BlackThorn] load.game_state", game_state_yaml, factory, game_bb);

    std::cout << "\n--- Tick + reset ---\n";

    auto loadOrAbort = [&](std::filesystem::path const& path,
                           Blackboard::Ptr bb) {
        auto result = bt::benchmark::loadTreeFromYaml(factory, path, bb);
        if (!result.isSuccess())
        {
            throw std::runtime_error(result.getError());
        }
        return result.moveValue();
    };

    auto parallel_tree = loadOrAbort(parallel_yaml, nullptr);
    benchBlackThornTick("[BlackThorn] tick.parallel_100_success",
                        *parallel_tree);

    auto sequence_tree = loadOrAbort(sequence_yaml, nullptr);
    benchBlackThornTick("[BlackThorn] tick.sequence_depth_50", *sequence_tree);

    auto patrol_tree = loadOrAbort(patrol_yaml, patrol_bb);
    patrol_tree->setBlackboard(patrol_bb);
    benchBlackThornTick("[BlackThorn] tick.patrol", *patrol_tree);

    auto game_tree = loadOrAbort(game_state_yaml, game_bb);
    game_tree->setBlackboard(game_bb);
    benchBlackThornTick("[BlackThorn] tick.game_state", *game_tree);

    std::cout << "\n--- Blackboard ---\n";

    auto bb_writes = loadOrAbort(bb_writes_yaml, nullptr);
    benchBlackThornYamlLoad(
        "[BlackThorn] load.blackboard_writes", bb_writes_yaml, factory);
    benchBlackThornTick("[BlackThorn] tick.blackboard_writes", *bb_writes);

    auto bb_reads_bb = std::make_shared<Blackboard>();
    benchBlackThornYamlLoad("[BlackThorn] load.blackboard_port_reads",
                            bb_reads_yaml,
                            factory,
                            bb_reads_bb);
    auto bb_reads = loadOrAbort(bb_reads_yaml, bb_reads_bb);
    bb_reads->setBlackboard(bb_reads_bb);
    benchBlackThornTick("[BlackThorn] tick.blackboard_port_reads", *bb_reads);

    auto bb_remap_bb = std::make_shared<Blackboard>();
    benchBlackThornYamlLoad("[BlackThorn] load.blackboard_subtree_remap",
                            bb_remap_yaml,
                            factory,
                            bb_remap_bb);
    auto bb_remap = loadOrAbort(bb_remap_yaml, bb_remap_bb);
    bb_remap->setBlackboard(bb_remap_bb);
    benchBlackThornTick("[BlackThorn] tick.blackboard_subtree_remap",
                        *bb_remap);

    auto bb_large_bb = std::make_shared<Blackboard>();
    benchBlackThornYamlLoad("[BlackThorn] load.blackboard_large",
                            bb_large_yaml,
                            factory,
                            bb_large_bb);
    auto bb_large = loadOrAbort(bb_large_yaml, bb_large_bb);
    bb_large->setBlackboard(bb_large_bb);
    benchBlackThornTick("[BlackThorn] tick.blackboard_large", *bb_large);

    std::cout << "\n--- Reset only ---\n";
    warmupBlackThorn(*patrol_tree);
    double const reset_ms = ::benchmark::runTimed(
        kResetIterations, [&]() { patrol_tree->reset(); });
    ::benchmark::printResult(
        "[BlackThorn] reset.patrol x1000", kResetIterations, reset_ms);

    std::cout << "\n--- Visualizer OFF vs ON (stub) ---\n";
    warmupBlackThorn(*patrol_tree);
    patrol_tree->setVisualizerClient(nullptr);
    double const tick_off_ms = ::benchmark::runTimed(kTickIterations, [&]() {
        static_cast<void>(patrol_tree->tick());
        patrol_tree->reset();
    });
    ::benchmark::printResult("[BlackThorn] tick.patrol.visualizer_off",
                             kTickIterations,
                             tick_off_ms);

    auto visualizer = std::make_shared<VisualizerClient>();
    visualizer->enableStubMode(true);
    patrol_tree->setVisualizerClient(visualizer);
    warmupBlackThorn(*patrol_tree);
    double const tick_on_ms = ::benchmark::runTimed(kTickIterations, [&]() {
        static_cast<void>(patrol_tree->tick());
        patrol_tree->reset();
    });
    ::benchmark::printResult(
        "[BlackThorn] tick.patrol.visualizer_on", kTickIterations, tick_on_ms);
}

void runBehaviorTreeCppBenchmarks()
{
    std::cout << "\n=== BehaviorTree.CPP ===\n";
    std::cout << "XML: " << ::benchmark::benchmarksRoot() / "xml" << "\n\n";

    auto factory = btcpp::benchmark::makeFactory();

    auto const parallel_xml =
        btcpp::benchmark::xmlPath("parallel_100_success.xml");
    auto const sequence_xml =
        btcpp::benchmark::xmlPath("sequence_depth_50.xml");
    auto const patrol_xml = btcpp::benchmark::xmlPath("patrol.xml");
    auto const game_state_xml = btcpp::benchmark::xmlPath("game_state.xml");
    auto const bb_writes_xml =
        btcpp::benchmark::xmlPath("blackboard_writes.xml");
    auto const bb_reads_xml =
        btcpp::benchmark::xmlPath("blackboard_port_reads.xml");
    auto const bb_remap_xml =
        btcpp::benchmark::xmlPath("blackboard_subtree_remap.xml");
    auto const bb_large_xml = btcpp::benchmark::xmlPath("blackboard_large.xml");

    std::cout << "--- XML load (createTreeFromFile) ---\n";
    benchBTXmlLoad("[BTCPP] load.parallel_100_success", parallel_xml, factory);
    benchBTXmlLoad("[BTCPP] load.sequence_depth_50", sequence_xml, factory);
    benchBTXmlLoad("[BTCPP] load.patrol", patrol_xml, factory);
    benchBTXmlLoad("[BTCPP] load.game_state", game_state_xml, factory);

    std::cout << "\n--- Tick + reset ---\n";
    auto parallel_tree =
        btcpp::benchmark::loadTreeFromXml(factory, parallel_xml);
    benchBTTick("[BTCPP] tick.parallel_100_success", parallel_tree);

    auto sequence_tree =
        btcpp::benchmark::loadTreeFromXml(factory, sequence_xml);
    benchBTTick("[BTCPP] tick.sequence_depth_50", sequence_tree);

    auto patrol_tree = btcpp::benchmark::loadTreeFromXml(factory, patrol_xml);
    benchBTTick("[BTCPP] tick.patrol", patrol_tree);

    auto game_tree = btcpp::benchmark::loadTreeFromXml(factory, game_state_xml);
    benchBTTick("[BTCPP] tick.game_state", game_tree);

    std::cout << "\n--- Blackboard ---\n";
    benchBTXmlLoad("[BTCPP] load.blackboard_writes", bb_writes_xml, factory);
    benchBTXmlLoad("[BTCPP] load.blackboard_port_reads", bb_reads_xml, factory);
    benchBTXmlLoad(
        "[BTCPP] load.blackboard_subtree_remap", bb_remap_xml, factory);
    benchBTXmlLoad("[BTCPP] load.blackboard_large", bb_large_xml, factory);

    auto bb_writes_tree =
        btcpp::benchmark::loadTreeFromXml(factory, bb_writes_xml);
    benchBTTick("[BTCPP] tick.blackboard_writes", bb_writes_tree);

    auto bb_reads_tree =
        btcpp::benchmark::loadTreeFromXml(factory, bb_reads_xml);
    benchBTTick("[BTCPP] tick.blackboard_port_reads", bb_reads_tree);

    auto bb_remap_tree =
        btcpp::benchmark::loadTreeFromXml(factory, bb_remap_xml);
    benchBTTick("[BTCPP] tick.blackboard_subtree_remap", bb_remap_tree);

    auto bb_large_tree =
        btcpp::benchmark::loadTreeFromXml(factory, bb_large_xml);
    benchBTTick("[BTCPP] tick.blackboard_large", bb_large_tree);

    std::cout << "\n--- Reset only ---\n";
    warmupBT(patrol_tree);
    double const reset_ms = ::benchmark::runTimed(
        kResetIterations, [&]() { patrol_tree.haltTree(); });
    ::benchmark::printResult(
        "[BTCPP] reset.patrol x1000", kResetIterations, reset_ms);

    std::cout << "\n--- Observer OFF vs ON ---\n";
    warmupBT(patrol_tree);
    double const tick_off_ms = ::benchmark::runTimed(kTickIterations, [&]() {
        patrol_tree.tickOnce();
        patrol_tree.haltTree();
    });
    ::benchmark::printResult(
        "[BTCPP] tick.patrol.observer_off", kTickIterations, tick_off_ms);

    BT::TreeObserver observer(patrol_tree);
    warmupBT(patrol_tree);
    double const tick_on_ms = ::benchmark::runTimed(kTickIterations, [&]() {
        patrol_tree.tickOnce();
        (void)observer;
        patrol_tree.haltTree();
    });
    ::benchmark::printResult(
        "[BTCPP] tick.patrol.observer_on", kTickIterations, tick_on_ms);
}

} // anonymous namespace

int main()
{
    std::cout << "BlackThorn vs BehaviorTree.CPP benchmarks\n\n";
    runBlackThornBenchmarks();
    runBehaviorTreeCppBenchmarks();
    return 0;
}
