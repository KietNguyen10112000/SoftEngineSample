#pragma once

#include "MainSystem/Scripting/Components/Script.h"

using namespace soft;

class ClimbingBar : public Script
{
	using Base = Script;
protected:
	SCRIPT_DEFAULT_METHOD(ClimbingBar);

	TRACEABLE_FRIEND();
	void Trace(Tracer* tracer)
	{
		Base::Trace(tracer);
	}

	float m_halfDistance = 1.0f;

public:
	void OnStart() override;
	void OnGUI() override;

protected:
	virtual void SerializeToJson(Serializer* serializer, json& j) const override;
	virtual void DeserializeFromJson(Serializer* serializer, const json& j) override;
	virtual Handle<ClassMetadata> GetMetadata(size_t sign) override;

public:
	inline const auto& GetHalfDistance() const
	{
		return m_halfDistance;
	}

};

