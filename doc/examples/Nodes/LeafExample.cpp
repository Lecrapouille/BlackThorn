#include "LeafExample.hpp"

#include "BlackThorn/BlackThorn.hpp"

#include <iostream>

class ReportEnemy final: public bt::Action
{
public:

    explicit ReportEnemy(bt::Blackboard::Ptr blackboard)
    {
        m_blackboard = std::move(blackboard);
    }

    bt::Status onRunning() override
    {
        auto target = m_blackboard->get<std::string>("target");
        if (!target)
        {
            std::cout << "[Leaf] No target assigned\n";
            return bt::Status::FAILURE;
        }
        std::cout << "[Leaf] Engaging " << *target << '\n';
        return bt::Status::SUCCESS;
    }
};

int leaf_example()
{
    using namespace bt;

    auto blackboard = std::make_shared<Blackboard>();
    blackboard->set<int>("battery", 75);
    blackboard->set<std::string>("target", "Drone-A");

    auto condition = Node::create<Condition>(
        Condition::Function([bb = blackboard]() {
            return bb->get<int>("battery").value_or(0) > 20;
        }),
        blackboard);

    auto report = Node::create<ReportEnemy>(blackboard);

    auto sequence = Node::create<Sequence>();
    sequence->addChild(std::move(condition));
    sequence->addChild(std::move(report));

    auto tree = Tree::create();
    tree->setBlackboard(blackboard);
    tree->setRoot(std::move(sequence));

    Status status = tree->tick();
    std::cout << "[Leaf] Result: " << to_string(status) << '\n';
    return status == Status::SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}
