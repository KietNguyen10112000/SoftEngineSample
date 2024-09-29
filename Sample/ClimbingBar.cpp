#include "ClimbingBar.h"

#include "Graphics/Graphics.h"
#include "Graphics/DebugGraphics.h"

void ClimbingBar::OnStart()
{

}

void ClimbingBar::OnGUI()
{
	auto debugGraphics = Graphics::Get()->GetDebugGraphics();
	if (debugGraphics)
	{
		auto& global = GetGameObject()->GetCommittedGlobalTransform();
		auto& pos = global.Position();
		auto right = global.Right().Normal();
		auto start = pos + m_halfDistance * right;
		auto end = pos - m_halfDistance * right;
		debugGraphics->DrawLineSegment(start, end, { 1.0f,0.2f,0.5f,0.5f }, 0.03f);
	}
}

void ClimbingBar::SerializeToJson(Serializer* serializer, json& j) const
{
	Base::SerializeToJson(serializer, j);

	j["HalfDistance"] = m_halfDistance;
}

void ClimbingBar::DeserializeFromJson(Serializer* serializer, const json& j)
{
	Base::DeserializeFromJson(serializer, j);

	if (j.contains("HalfDistance"))
	{
		m_halfDistance = j["HalfDistance"];
	}
}

Handle<ClassMetadata> ClimbingBar::GetMetadata(size_t sign)
{
	auto metadata = mheap::New<ClassMetadata>(GetClassName(), this);

	auto accessor = Accessor(
		"Half Length",
		1,
		[](const Variant& input, UnknownAddress& var, Serializable* instance) -> void
		{
			auto script = (ClimbingBar*)instance;
			script->m_halfDistance = input.As<float>();
		},

		[](UnknownAddress& var, Serializable* instance) -> Variant
		{
			auto script = (ClimbingBar*)instance;
			return Variant::Of(script->m_halfDistance);
		},
		this
	);
	metadata->AddProperty(accessor);

	return metadata;
}
