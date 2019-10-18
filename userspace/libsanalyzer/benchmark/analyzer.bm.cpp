#include <benchmark/benchmark.h>
#include <analyzer.h>
#include <run.h>
#include <sinsp_mock.h>

using namespace test_helpers;

namespace
{
sinsp_analyzer::flush_queue g_queue(2000);
audit_tap_handler_dummy g_audit_handler;
null_secure_audit_handler g_secure_handler;
}

void one_hundred_thousand_reads(benchmark::State& state)
{
	for (auto _ : state)
	{
		shared_ptr<sinsp_mock> inspector(new sinsp_mock());
		inspector->build_event().count(100000).type(PPME_SYSCALL_READ_E).commit();

		internal_metrics::sptr_t int_metrics = std::make_shared<internal_metrics>();

		sinsp_analyzer analyzer(inspector.get(),
		                        "/" /*root dir*/,
		                        int_metrics,
		                        g_audit_handler,
		                        g_secure_handler,
		                        &g_queue);

		run_sinsp_with_analyzer(*inspector, analyzer);

		// For legacy reasons, the inspector must be deleted before the
		// analyzer.
		inspector.reset();
	}
}
BENCHMARK(one_hundred_thousand_reads)->Unit(benchmark::kMillisecond);



