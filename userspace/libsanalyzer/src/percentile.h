#pragma once

#include "sinsp.h"
#include "sinsp_int.h"
#include "tdigest/tdigest.h"

// fwd declarations
namespace draiosproto {
class counter_percentile_data;
};

class percentile
{
public:
	typedef std::map<int, double> p_map_type;

	percentile() = delete;
	percentile(const std::set<double>& pctls, double eps = .02);
	~percentile();

	percentile(const percentile& other);
	percentile& operator=(const percentile &other);

	template <typename T>
	void add(T val)
	{
		alloc_tdigest_if_needed();
		m_digest->add(val);
		++m_num_samples;
	}

	template <typename T>
	void copy(const std::vector<T>& val)
	{
		reset();
		for(auto &v: val) {
			add(v);
		}
	}

	void merge(const percentile *other)
	{
		if (other->sample_count()) {
			alloc_tdigest_if_needed();
			m_digest->merge(other->m_digest.get());
			m_num_samples += other->sample_count();
		}
	}

	p_map_type percentiles();

	template <typename P, typename C1, typename C2>
	void to_protobuf(P* proto, C1* (P::*add_pctl)(), C2* (P::*data)())
	{
		to_protobuf(percentiles(), proto, add_pctl);
		if (data != nullptr) {
			serialize((proto->*data)());
		}
		reset();
	}

	template <typename P, typename C>
	static void to_protobuf(const p_map_type& pm, P* proto, C* (P::*add_func)())
	{
		for(const auto& p : pm)
		{
			C* cp = (proto->*add_func)();
			cp->set_percentile(p.first);
			cp->set_value(p.second);
		}
	}

	void reset();
	inline std::vector<double> get_percentiles() const;
	uint32_t sample_count() const
	{
		return m_num_samples;
	}

	void dump_samples();
	inline void flush() const;
	void serialize(draiosproto::counter_percentile_data *pdata) const;
	void deserialize(const draiosproto::counter_percentile_data *pdata);

private:
	inline void init(const std::vector<double> &percentiles, double eps = 0.02);
	inline void copy(const percentile& other);
	inline void destroy(std::vector<double>* percentiles = nullptr);
	void alloc_tdigest_if_needed(void);

	std::vector<double> m_percentiles;
	double m_eps;
	std::unique_ptr<tdigest::TDigest> m_digest;
	size_t m_num_samples = 0;
};