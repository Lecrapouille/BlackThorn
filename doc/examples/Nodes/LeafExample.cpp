#include "LeafExample.hpp"

#include "BlackThorn/BlackThorn.hpp"

#include <iostream>

class ReportEnemy final: public bt::CallbackLeaf
{
public:

    explicit ReportEnemy(bt::Blackboard::Ptr blackboard)
        : CallbackLeaf(
              [bb = blackboard]() {
                  auto target = bb->get<std::string>("target");
                  if (!target)
                  {
                      std::cout << "[Leaf] No target assigned\n";
                      return bt::Status::FAILURE;
                  }
                  std::cout << "[Leaf] Engaging " << *target << '\n';
                  return bt::Status::SUCCESS;
              },
              std::move(blackboard))
    {
    }
};

int leaf_example()
{
    using namespace bt;

    auto blackboard = std::make_shared<Blackboard>();
    blackboard->set<int>("battery", 75);
    blackboard->set<std::string>("target", "Drone-A");

    auto tree = Tree::create();
    tree->setBlackboard(blackboard);
    auto& sequence = tree->createRoot<Sequence>();
    sequence.addChild<Condition>(
        Condition::Function([bb = blackboard]() {
            return bb->get<int>("battery").value_or(0) > 20;
        }),
        blackboard);
    sequence.addChild<ReportEnemy>(blackboard);

    Status status = tree->tick();
    std::cout << "[Leaf] Result: " << to_string(status) << '\n';
    return status == Status::SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}
