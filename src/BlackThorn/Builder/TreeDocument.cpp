/**
 * @file TreeDocument.cpp
 * @brief Parsed behavior tree YAML with separate instantiate step.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "BlackThorn/Builder/TreeDocument.hpp"

#include "BlackThorn/Blackboard/Serializer.hpp"
#include "BlackThorn/Builder/Builder.hpp"

namespace bt {

namespace {

// ----------------------------------------------------------------------------
//! \brief Build the registry of reusable subtrees if provided in YAML input.
// ----------------------------------------------------------------------------
static robotik::Return<SubTreeRegistry>
buildSubTreeRegistry(YamlNode const& p_root)
{
    SubTreeRegistry registry;

    if (!p_root.hasKey("SubTrees"))
    {
        return robotik::Return<SubTreeRegistry>::success(std::move(registry));
    }

    YamlNode const subtrees = p_root.child("SubTrees");
    if (!subtrees.isMap())
    {
        return robotik::Return<SubTreeRegistry>::error(
            "'SubTrees' section must be a map of name -> node definitions");
    }

    subtrees.forEachMap([&](std::string_view p_name, YamlNode p_def) {
        registry.definitions.emplace(std::string(p_name), p_def);
    });

    return robotik::Return<SubTreeRegistry>::success(std::move(registry));
}

} // namespace

// ---------------------------------------------------------------------------
robotik::Return<std::shared_ptr<TreeDocument>>
TreeDocument::parseFile(std::string const& p_path)
{
    auto yamlResult = YamlDocument::parseFile(p_path);
    if (!yamlResult)
    {
        return robotik::Return<std::shared_ptr<TreeDocument>>::error(
            yamlResult.getError());
    }

    auto doc = std::make_shared<TreeDocument>();
    doc->m_yaml = yamlResult.moveValue();

    auto registryResult = buildSubTreeRegistry(doc->m_yaml.root());
    if (!registryResult)
    {
        return robotik::Return<std::shared_ptr<TreeDocument>>::error(
            registryResult.getError());
    }

    doc->m_subtrees = registryResult.moveValue();
    doc->m_hasBlackboard = doc->m_yaml.hasKey("Blackboard");

    return robotik::Return<std::shared_ptr<TreeDocument>>::success(
        std::move(doc));
}

// ---------------------------------------------------------------------------
robotik::Return<std::shared_ptr<TreeDocument>>
TreeDocument::parseText(std::string p_text)
{
    auto yamlResult = YamlDocument::parseText(std::move(p_text));
    if (!yamlResult)
    {
        return robotik::Return<std::shared_ptr<TreeDocument>>::error(
            yamlResult.getError());
    }

    auto doc = std::make_shared<TreeDocument>();
    doc->m_yaml = yamlResult.moveValue();

    auto registryResult = buildSubTreeRegistry(doc->m_yaml.root());
    if (!registryResult)
    {
        return robotik::Return<std::shared_ptr<TreeDocument>>::error(
            registryResult.getError());
    }

    doc->m_subtrees = registryResult.moveValue();
    doc->m_hasBlackboard = doc->m_yaml.hasKey("Blackboard");

    return robotik::Return<std::shared_ptr<TreeDocument>>::success(
        std::move(doc));
}

// ---------------------------------------------------------------------------
robotik::Return<Tree::Ptr> TreeDocument::instantiate(
    NodeFactory const& p_factory,
    Blackboard::Ptr p_blackboard,
    BuilderOptions p_options) const
{
    if (!m_yaml.hasKey("BehaviorTree"))
    {
        return robotik::Return<Tree::Ptr>::error(
            "Missing 'BehaviorTree' node in YAML document");
    }

    Blackboard::Ptr blackboard =
        p_blackboard ? p_blackboard : std::make_shared<Blackboard>();

    if (p_options.mergeBlackboard && m_hasBlackboard)
    {
        BlackboardSerializer::load(*blackboard,
                                   m_yaml.root().child("Blackboard"),
                                   blackboard.get());
    }

    auto tree = Tree::create();
    tree->setBlackboard(blackboard);

    YamlNode const behaviorTree = m_yaml.root().child("BehaviorTree");

    SubTreeRegistry const* registryPtr =
        m_subtrees.definitions.empty() ? nullptr : &m_subtrees;

    auto nodeResult = Builder::parseYAMLNode(*tree,
                                             p_factory,
                                             behaviorTree,
                                             blackboard,
                                             registryPtr,
                                             p_options);
    if (!nodeResult)
    {
        return robotik::Return<Tree::Ptr>::error(nodeResult.getError());
    }

    tree->setRootIndex(nodeResult.getValue());
    return robotik::Return<Tree::Ptr>::success(std::move(tree));
}

} // namespace bt
