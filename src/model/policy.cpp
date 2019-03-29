#include <torch/torch.h>

#include "cpprl/model/policy.h"
#include "cpprl/model/mlp_base.h"
#include "cpprl/model/output_layers.h"
#include "cpprl/spaces.h"
#include "third_party/doctest.h"

using namespace torch;

namespace cpprl
{
PolicyImpl::PolicyImpl(ActionSpace action_space, std::shared_ptr<NNBase> base)
    : base(base)
{
    int num_outputs;
    if (action_space.type == "Discrete")
    {
        num_outputs = action_space.shape[0];
        output_layer = std::make_shared<CategoricalOutput>(
            base->get_hidden_size(), num_outputs);
    }
    else if (action_space.type == "Box")
    {
        // num_outputs = action_space.shape[0];
        // self.dist = DiagGaussian(self.base.output_size, num_outputs)
    }
    else if (action_space.type == "MultiBinary")
    {
        // num_outputs = action_space.shape[0];
        // self.dist = Bernoulli(self.base.output_size, num_outputs)
    }
    else
    {
        throw std::exception();
    }
}

std::vector<torch::Tensor> PolicyImpl::act(torch::Tensor inputs,
                                           torch::Tensor rnn_hxs,
                                           torch::Tensor masks)
{
    auto base_output = base->forward(inputs, rnn_hxs, masks);
    auto dist = output_layer->forward(base_output[1]);

    auto action = dist->sample();

    auto action_log_probs = dist->log_prob(action);
    return {base_output[0], // value
            action.unsqueeze(-1),
            action_log_probs.unsqueeze(-1),
            base_output[2]}; // rnn_hxs
}

std::vector<torch::Tensor> PolicyImpl::evaluate_actions(torch::Tensor /*inputs*/,
                                                        torch::Tensor /*rnn_hxs*/,
                                                        torch::Tensor /*masks*/,
                                                        torch::Tensor /*actions*/)
{
    return std::vector<torch::Tensor>();
}

torch::Tensor PolicyImpl::get_values(torch::Tensor /*inputs*/,
                                     torch::Tensor /*rnn_hxs*/,
                                     torch::Tensor /*masks*/)
{
    return torch::Tensor();
}

TEST_CASE("Policy")
{
    auto base = std::make_shared<MlpBase>(3, true, 10);
    auto policy = Policy(ActionSpace{"Discrete", {5}}, base);

    SUBCASE("Sanity checks")
    {
        CHECK(policy->is_recurrent() == true);
        CHECK(policy->get_hidden_size() == 10);
    }

    SUBCASE("act() output tensors are correct shapes")
    {
        auto inputs = torch::rand({4, 3});
        auto rnn_hxs = torch::rand({4, 10});
        auto masks = torch::zeros({4, 1});
        auto outputs = policy->act(inputs, rnn_hxs, masks);

        REQUIRE(outputs.size() == 4);

        // Value
        INFO("Value: \n"
             << outputs[0] << "\n");
        CHECK(outputs[0].size(0) == 4);
        CHECK(outputs[0].size(1) == 1);

        // Actions
        INFO("Actions: \n"
             << outputs[1] << "\n");
        CHECK(outputs[1].size(0) == 4);
        CHECK(outputs[1].size(1) == 1);

        // Log probs
        INFO("Log probs: \n"
             << outputs[2] << "\n");
        CHECK(outputs[2].size(0) == 4);
        CHECK(outputs[2].size(1) == 1);

        // Hidden states
        INFO("Hidden states: \n"
             << outputs[3] << "\n");
        CHECK(outputs[3].size(0) == 4);
        CHECK(outputs[3].size(1) == 10);
    }

    SUBCASE("evaluate_actions() output tensors are correct shapes")
    {
        auto inputs = torch::rand({4, 3});
        auto rnn_hxs = torch::rand({4, 10});
        auto masks = torch::zeros({4, 1});
        auto actions = torch::rand({4, 5});
        auto outputs = policy->evaluate_actions(inputs, rnn_hxs, masks, actions);

        REQUIRE(outputs.size() == 4);

        // Value
        CHECK(outputs[0].size(0) == 4);
        CHECK(outputs[0].size(1) == 1);

        // Log probs
        CHECK(outputs[1].size(0) == 4);
        CHECK(outputs[1].size(1) == 5);

        // Entropy
        CHECK(outputs[2].size(0) == 1);

        // Hidden states
        CHECK(outputs[3].size(0) == 4);
        CHECK(outputs[3].size(1) == 10);
    }

    SUBCASE("get_values() output tensor is correct shapes")
    {
        auto inputs = torch::rand({4, 3});
        auto rnn_hxs = torch::rand({4, 10});
        auto masks = torch::zeros({4, 1});
        auto outputs = policy->get_values(inputs, rnn_hxs, masks);

        // Value
        CHECK(outputs.size(0) == 4);
        CHECK(outputs.size(1) == 1);
    }
}
}