#pragma once
#include <string>

/// @brief コンソール変数インターフェースクラス
class IConVar {
public:
	virtual ~IConVar() = default;

	[[nodiscard]] virtual std::string        GetValueAsString() const = 0;
	[[nodiscard]] virtual float              GetValueAsFloat() const = 0;
	[[nodiscard]] virtual double             GetValueAsDouble() const = 0;
	[[nodiscard]] virtual int                GetValueAsInt() const = 0;
	[[nodiscard]] virtual bool               GetValueAsBool() const = 0;
	[[nodiscard]] virtual Vec3               GetValueAsVec3() const = 0;
	[[nodiscard]] virtual std::string        GetTypeAsString() const = 0;
	[[nodiscard]] virtual const std::string& GetName() const = 0;
	[[nodiscard]] virtual const std::string& GetHelp() const = 0;

	virtual void SetValueFromFloat(float newValue) = 0;
	virtual void SetValueFromDouble(double newValue) = 0;
	virtual void SetValueFromInt(int newValue) = 0;
	virtual void SetValueFromBool(bool newValue) = 0;
	virtual void SetValueFromString(const std::string& newValue) = 0;

	virtual void Toggle() = 0;

	virtual void DrawImGui() = 0;
};
