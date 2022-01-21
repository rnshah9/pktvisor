#include "CoreRegistry.h"
#include "HandlerManager.h"
#include "InputModulePlugin.h"
#include "InputStream.h"
#include "InputStreamManager.h"
#include "MockInputStream.h"
#include "Policies.h"
#include "handlers/static_plugins.h"
#include <catch2/catch.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

using namespace visor;

auto policies_config = R"(
version: "1.0"

visor:
  taps:
    anycast:
      input_type: mock
      config:
        iface: eth0
  policies:
    # policy name and description
    default_view:
      kind: collection
#      description: "a mock view of anycast traffic"
      # input stream to create based on the given tap and optional filter config
      input:
        # this must reference a tap name, or application of the policy will fail
        tap: anycast
        input_type: mock
        config:
          sample: value
        filter:
          bpf: "tcp or udp"
      # stream handlers to attach to this input stream
      # these decide exactly which data to summarize and expose for collection
      handlers:
        # default configuration for the stream handlers
        window_config:
          num_periods: 5
          deep_sample_rate: 100
        modules:
          # the keys at this level are unique identifiers
          default_net:
            type: net
          default_dns:
            type: dns
#            window_config:
#              max_deep_sample: 75
          special_domain:
            type: dns
            filter:
              only_qname_suffix:
                - ".google.com"
                - ".ns1.com"
                - "slack.com"
)";

auto policies_config_hseq = R"(
version: "1.0"

visor:
  taps:
    anycast:
      input_type: mock
      config:
        iface: eth0
  policies:
   default_view:
     kind: collection
     input:
       tap: anycast
       input_type: mock
       config:
         sample: value
     handlers:
        window_config:
          num_periods: 5
          deep_sample_rate: 100
        modules:
          - default_dns:
            type: dns
            config:
              in_key: in_value
          - default_net:
            type: net
)";

auto policies_config_same_input = R"(
version: "1.0"

visor:
  policies:
    # policy name and description
    same_input:
      kind: collection
      input:
        # this must reference a tap name, or application of the policy will fail
        tap: anycast
        input_type: mock
        config:
          sample: value
        filter:
          bpf: "tcp or udp"
      handlers:
        window_config:
          num_periods: 5
          deep_sample_rate: 100
        modules:
          net:
            type: net
)";

auto policies_config_bad1 = R"(
visor:
  policies:
    missing:
)";

auto policies_config_bad2 = R"(
version: "1.0"

visor:
  taps:
    anycast:
      input_type: mock
      config:
        iface: eth0
  policies:
    default_view:
      kind: collection
      input:
        tap: nonexist
        input_type: mock
)";

auto policies_config_bad3 = R"(
version: "1.0"

visor:
  taps:
    anycast:
      input_type: mock
      config:
        iface: eth0
  policies:
    default_view:
      kind: collection
      input:
        tap: anycast
        input_type: mock
        filter:
          bpf:
            badmap: "bad value"
)";

auto policies_config_bad4 = R"(
version: "1.0"

visor:
  taps:
    anycast:
      input_type: mock
      config:
        iface: eth0
  policies:
    default_view:
      kind: collection
      input:
        tap: anycast
        input_type: mock
        config:
          except_on_start: true
      handlers:
        modules:
          default_net:
            type: net
)";
auto policies_config_bad5 = R"(
version: "1.0"

visor:
  taps:
    anycast:
      input_type: mock
      config:
        iface: eth0
  policies:
    default_view:
      kind: collection
      input:
        tap: anycast
        input_type: wrong_type
)";
auto policies_config_bad6 = R"(
version: "1.0"

visor:
  taps:
    anycast:
      input_type: mock
      config:
        iface: eth0
  policies:
    default_view:
      kind: unknown_kind
      input:
        tap: anycast
        input_type: mock
)";

auto policies_config_bad7 = R"(
version: "1.0"

visor:
  taps:
    anycast:
      input_type: mock
      config:
        iface: eth0
  policies:
    default_view:
      kind: collection
      input:
        tap: anycast
        input_type: mock
      handlers:
        window_config:
          num_periods: 5
          deep_sample_rate: 100
        modules:
           default_net:
            type: net2
)";

auto policies_config_bad8 = R"(
version: "1.0"

visor:
  taps:
    anycast:
      input_type: mock
      config:
        iface: eth0
  policies:
    default_view:
      kind: collection
      input:
        tap: anycast
        input_type: mock
      handlers:
        window_config:
          num_periods: 5
          deep_sample_rate: 100
        modules:
          default_net:
             type: net
          default_dns:
             config: dns
)";

auto policies_config_bad9 = R"(
version: "1.0"

visor:
  taps:
    anycast:
      input_type: mock
      config:
        iface: eth0
  policies:
    default_view:
      kind: collection
      input:
        tap: anycast
        input_type: mock
      handlers:
        window_config:
          num_periods: 5
          deep_sample_rate: 100
        modules:
          default_net:
             type: net
          default_dns:
             type: dns
             config: not_a_map
)";

auto policies_config_hseq_bad1 = R"(
version: "1.0"

visor:
  taps:
    anycast:
      input_type: mock
      config:
        iface: eth0
  policies:
    default_view:
      kind: collection
      input:
        tap: anycast
        input_type: mock
      handlers:
        window_config:
          num_periods: 5
          deep_sample_rate: 100
        modules:
          - default_net:
            type: net
          - default_dns:
            type: dns
)";

auto policies_config_hseq_bad2 = R"(
version: "1.0"

visor:
  taps:
    anycast:
      input_type: mock
      config:
        iface: eth0
  policies:
    default_view:
      kind: collection
      input:
        tap: anycast
        input_type: mock
      handlers:
        window_config:
          num_periods: 5
          deep_sample_rate: 100
        modules:
           default_dns:
            type: dns
          - default_net:
            type: net
)";

TEST_CASE("Policies", "[policies]")
{

    SECTION("Good Config happy path")
    {
        CoreRegistry registry;
        registry.start(nullptr);
        YAML::Node config_file = YAML::Load(policies_config);

        CHECK(config_file["visor"]["policies"]);
        CHECK(config_file["visor"]["policies"].IsMap());

        REQUIRE_NOTHROW(registry.tap_manager()->load(config_file["visor"]["taps"], true));
        REQUIRE_NOTHROW(registry.policy_manager()->load(config_file["visor"]["policies"]));

        REQUIRE(registry.policy_manager()->module_exists("default_view"));
        auto [policy, lock] = registry.policy_manager()->module_get_locked("default_view");
        CHECK(policy->name() == "default_view");
        CHECK(policy->input_stream()->name() == "anycast-7c1e3de3e177c3ea");
        CHECK(policy->input_stream()->config_get<std::string>("bpf") == "tcp or udp"); // TODO this will move to filter member variable
        CHECK(policy->input_stream()->config_get<std::string>("sample") == "value");
        CHECK(policy->modules()[0]->name() == "default_view-default_net");
        CHECK(policy->modules()[1]->name() == "default_view-default_dns");
        CHECK(policy->modules()[2]->name() == "default_view-special_domain");
        CHECK(policy->modules()[2]->config_get<Configurable::StringList>("only_qname_suffix")[0] == ".google.com");
        // TODO check window config settings made it through
        CHECK(policy->input_stream()->running());
        CHECK(policy->modules()[0]->running());
        CHECK(policy->modules()[1]->running());
        CHECK(policy->modules()[2]->running());
    }

    SECTION("Good Config sequence modules")
    {
        CoreRegistry registry;
        registry.start(nullptr);
        YAML::Node config_file = YAML::Load(policies_config_hseq);

        CHECK(config_file["visor"]["policies"]);
        CHECK(config_file["visor"]["policies"].IsMap());
        REQUIRE_NOTHROW(registry.tap_manager()->load(config_file["visor"]["taps"], true));
        REQUIRE_NOTHROW(registry.policy_manager()->load(config_file["visor"]["policies"]));

        REQUIRE(registry.policy_manager()->module_exists("default_view"));
        auto [policy, lock] = registry.policy_manager()->module_get_locked("default_view");
        CHECK(policy->name() == "default_view");
        CHECK(policy->input_stream()->name() == "anycast-f2fb471f6cb587fd");
        CHECK(policy->modules()[0]->name() == "default_view-default_dns");
        CHECK(policy->modules()[1]->name() == "default_view-default_net");
        CHECK(policy->input_stream()->running());
        CHECK(policy->modules()[0]->running());
        CHECK(policy->modules()[1]->running());
    }

    // TODO multiple collection policies in the same yaml

    SECTION("Duplicate")
    {
        CoreRegistry registry;
        registry.start(nullptr);
        YAML::Node config_file = YAML::Load(policies_config);

        REQUIRE_NOTHROW(registry.tap_manager()->load(config_file["visor"]["taps"], true));
        REQUIRE_NOTHROW(registry.policy_manager()->load(config_file["visor"]["policies"]));
        REQUIRE_THROWS_WITH(registry.policy_manager()->load(config_file["visor"]["policies"]), "policy with name 'default_view' already defined");

        REQUIRE(registry.policy_manager()->module_exists("default_view"));
        auto [policy, lock] = registry.policy_manager()->module_get_locked("default_view");
        CHECK(policy->name() == "default_view");
    }

    SECTION("Bad Config")
    {
        CoreRegistry registry;
        YAML::Node config_file = YAML::Load(policies_config_bad1);

        REQUIRE_THROWS_WITH(registry.policy_manager()->load(config_file["visor"]["policies"]), "expecting policy configuration map");
    }

    SECTION("Bad Config: invalid tap")
    {
        CoreRegistry registry;
        YAML::Node config_file = YAML::Load(policies_config_bad2);

        REQUIRE_THROWS_WITH(registry.policy_manager()->load(config_file["visor"]["policies"]), "tap 'nonexist' does not exist");
    }

    SECTION("Bad Config: invalid tap config")
    {
        CoreRegistry registry;
        registry.start(nullptr);
        YAML::Node config_file = YAML::Load(policies_config_bad3);

        REQUIRE_NOTHROW(registry.tap_manager()->load(config_file["visor"]["taps"], true));
        REQUIRE_THROWS_WITH(registry.policy_manager()->load(config_file["visor"]["policies"]), "invalid input filter for tap 'anycast': invalid value for key: bpf");
    }

    SECTION("Bad Config: exception on input start")
    {
        CoreRegistry registry;
        registry.start(nullptr);
        YAML::Node config_file = YAML::Load(policies_config_bad4);

        REQUIRE_NOTHROW(registry.tap_manager()->load(config_file["visor"]["taps"], true));
        REQUIRE_THROWS_WITH(registry.policy_manager()->load(config_file["visor"]["policies"]), "policy [default_view] failed to start: mock error on start");
    }

    SECTION("Bad Config: mis-matched input_type on tap")
    {
        CoreRegistry registry;
        registry.start(nullptr);
        YAML::Node config_file = YAML::Load(policies_config_bad5);

        REQUIRE_NOTHROW(registry.tap_manager()->load(config_file["visor"]["taps"], true));
        REQUIRE_THROWS_WITH(registry.policy_manager()->load(config_file["visor"]["policies"]), "unable to instantiate tap 'anycast': input_type for policy specified tap 'anycast' doesn't match tap's defined input type: wrong_type/mock");
    }

    SECTION("Bad Config: bad policy kind")
    {
        CoreRegistry registry;
        registry.start(nullptr);
        YAML::Node config_file = YAML::Load(policies_config_bad6);

        REQUIRE_NOTHROW(registry.tap_manager()->load(config_file["visor"]["taps"], true));
        REQUIRE_THROWS_WITH(registry.policy_manager()->load(config_file["visor"]["policies"]), "unknown policy kind: unknown_kind");
    }

    SECTION("Bad Config: invalid handler")
    {
        CoreRegistry registry;
        registry.start(nullptr);
        YAML::Node config_file = YAML::Load(policies_config_bad7);

        REQUIRE_NOTHROW(registry.tap_manager()->load(config_file["visor"]["taps"], true));
        REQUIRE_THROWS_WITH(registry.policy_manager()->load(config_file["visor"]["policies"]), "Policy 'default_view' requires stream handler type 'net2' which is not available");
    }

    SECTION("Bad Config: invalid handler module")
    {
        CoreRegistry registry;
        registry.start(nullptr);
        YAML::Node config_file = YAML::Load(policies_config_bad7);

        REQUIRE_NOTHROW(registry.tap_manager()->load(config_file["visor"]["taps"], true));
        REQUIRE_THROWS_WITH(registry.policy_manager()->load(config_file["visor"]["policies"]), "Policy 'default_view' requires stream handler type 'net2' which is not available");
    }

    SECTION("Bad Config: handler module without a type")
    {
        CoreRegistry registry;
        registry.start(nullptr);
        YAML::Node config_file = YAML::Load(policies_config_bad8);

        REQUIRE_NOTHROW(registry.tap_manager()->load(config_file["visor"]["taps"], true));
        REQUIRE_THROWS_WITH(registry.policy_manager()->load(config_file["visor"]["policies"]), "missing or invalid stream handler type at key 'type'");
    }

    SECTION("Bad Config: handler module not map config")
    {
        CoreRegistry registry;
        registry.start(nullptr);
        YAML::Node config_file = YAML::Load(policies_config_bad9);

        REQUIRE_NOTHROW(registry.tap_manager()->load(config_file["visor"]["taps"], true));
        REQUIRE_THROWS_WITH(registry.policy_manager()->load(config_file["visor"]["policies"]), "stream handler configuration is not a map");
    }

    SECTION("Bad Config: invalid handler modules order")
    {
        CoreRegistry registry;
        registry.start(nullptr);
        YAML::Node config_file = YAML::Load(policies_config_hseq_bad1);

        REQUIRE_NOTHROW(registry.tap_manager()->load(config_file["visor"]["taps"], true));
        REQUIRE_THROWS_WITH(registry.policy_manager()->load(config_file["visor"]["policies"]), "DnsStreamHandler: unsupported upstream chained stream handler default_view-default_net");
    }

    SECTION("Bad Config: invalid handler modules YAML type")
    {
        CoreRegistry registry;
        registry.start(nullptr);
        REQUIRE_THROWS_WITH(YAML::Load(policies_config_hseq_bad2), "yaml-cpp: error at line 23, column 11: end of map not found");
    }

    SECTION("Roll Back")
    {
        CoreRegistry registry;
        registry.start(nullptr);
        YAML::Node config_file = YAML::Load(policies_config);

        CHECK(config_file["visor"]["policies"]);
        CHECK(config_file["visor"]["policies"].IsMap());

        REQUIRE_NOTHROW(registry.tap_manager()->load(config_file["visor"]["taps"], true));

        // force a roll back by creating a conflict with a handler module name that already exists
        Config config;
        auto input_stream = registry.input_plugins()["mock"]->instantiate("mymock", &config);
        auto mod = registry.handler_plugins()["net"]->instantiate("default_view-default_net", input_stream.get(), &config);
        registry.handler_manager()->module_add(std::move(mod));
        REQUIRE_THROWS_WITH(registry.policy_manager()->load(config_file["visor"]["policies"]), "policy [default_view-default_net] creation failed (handler: default_view): module name 'default_view-default_net' already exists");

        // ensure the modules were rolled back
        REQUIRE(!registry.policy_manager()->module_exists("default_view"));
        REQUIRE(!registry.input_manager()->module_exists("anycast-7c1e3de3e177c3ea"));
    }
    SECTION("Good Config, test stop()")
    {
        CoreRegistry registry;
        registry.start(nullptr);
        YAML::Node config_file = YAML::Load(policies_config);

        CHECK(config_file["visor"]["policies"]);
        CHECK(config_file["visor"]["policies"].IsMap());

        REQUIRE_NOTHROW(registry.tap_manager()->load(config_file["visor"]["taps"], true));
        REQUIRE_NOTHROW(registry.policy_manager()->load(config_file["visor"]["policies"]));

        REQUIRE(registry.policy_manager()->module_exists("default_view"));
        auto [policy, lock] = registry.policy_manager()->module_get_locked("default_view");
        CHECK(policy->name() == "default_view");
        CHECK(policy->input_stream()->running());
        CHECK(policy->modules()[0]->running());
        CHECK(policy->modules()[1]->running());
        CHECK(policy->modules()[2]->running());
        policy->stop();
        CHECK(!policy->input_stream()->running());
        CHECK(!policy->modules()[0]->running());
        CHECK(!policy->modules()[1]->running());
        CHECK(!policy->modules()[2]->running());
        lock.unlock();
        REQUIRE_NOTHROW(registry.policy_manager()->module_remove("default_view"));
    }

    SECTION("Good Config, test remove policy and add again")
    {
        CoreRegistry registry;
        registry.start(nullptr);
        YAML::Node config_file = YAML::Load(policies_config);

        CHECK(config_file["visor"]["policies"]);
        CHECK(config_file["visor"]["policies"].IsMap());

        REQUIRE_NOTHROW(registry.tap_manager()->load(config_file["visor"]["taps"], true));
        REQUIRE_NOTHROW(registry.policy_manager()->load(config_file["visor"]["policies"]));

        REQUIRE(registry.policy_manager()->module_exists("default_view"));
        auto [policy, lock] = registry.policy_manager()->module_get_locked("default_view");
        CHECK(policy->name() == "default_view");
        CHECK(policy->input_stream()->running());
        CHECK(policy->modules()[0]->running());
        CHECK(policy->modules()[1]->running());
        CHECK(policy->modules()[2]->running());
        lock.unlock();

        REQUIRE_NOTHROW(registry.policy_manager()->remove_policy("default_view"));

        REQUIRE_NOTHROW(registry.policy_manager()->load(config_file["visor"]["policies"]));
        REQUIRE(registry.policy_manager()->module_exists("default_view"));
        auto [new_policy, new_lock] = registry.policy_manager()->module_get_locked("default_view");
        CHECK(new_policy->name() == "default_view");
        CHECK(new_policy->input_stream()->running());
        CHECK(new_policy->modules()[0]->running());
        CHECK(new_policy->modules()[1]->running());
        CHECK(new_policy->modules()[2]->running());
        new_lock.unlock();
        REQUIRE_NOTHROW(registry.policy_manager()->remove_policy("default_view"));
    }

    SECTION("Good Config, policies with same tap and input")
    {
        CoreRegistry registry;
        registry.start(nullptr);
        YAML::Node config_file = YAML::Load(policies_config);

        CHECK(config_file["visor"]["policies"]);
        CHECK(config_file["visor"]["policies"].IsMap());

        REQUIRE_NOTHROW(registry.tap_manager()->load(config_file["visor"]["taps"], true));
        REQUIRE_NOTHROW(registry.policy_manager()->load(config_file["visor"]["policies"]));

        REQUIRE(registry.policy_manager()->module_exists("default_view"));
        auto [policy, lock] = registry.policy_manager()->module_get_locked("default_view");
        CHECK(policy->name() == "default_view");
        CHECK(policy->input_stream()->name() == "anycast-7c1e3de3e177c3ea");
        lock.unlock();

        YAML::Node config_file2 = YAML::Load(policies_config_same_input);
        CHECK(config_file2["visor"]["policies"]);
        CHECK(config_file2["visor"]["policies"].IsMap());

        REQUIRE_NOTHROW(registry.policy_manager()->load(config_file2["visor"]["policies"]));

        REQUIRE(registry.policy_manager()->module_exists("same_input"));
        auto [policy2, lock2] = registry.policy_manager()->module_get_locked("same_input");
        CHECK(policy2->name() == "same_input");
        CHECK(policy2->input_stream()->name() == "anycast-7c1e3de3e177c3ea");
        lock2.unlock();
    }
}
