#pragma once

#include "../Core/Types.h"
#include "../Core/Logger.h"
#include <string>
#include <string_view>
#include <functional>
#include <unordered_map>
#include <variant>
#include <vector>
#include <memory>
#include <optional>
#include <filesystem>


//sol2の警告を抑制
#pragma warning(push,0)
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#pragma warning(pop)

namespace UnoEngine {

	// スクリプトプロパティの型
	using ScriptPropertyValue = std::variant<
		bool,
		int32,
		float,
		std::string
	>;

	// スクリプトプロパティ情報
	struct ScriptProperty {
		std::string name;
		ScriptPropertyValue value;
		ScriptPropertyValue defaultValue;
	};

	// Luaスクリプトエラー情報
	struct LuaError {
		std::string message;
		std::string scriptPath;
		int32 line = -1;
		std::string stackTrace;
	};

	// Lua状態管理クラス
	class LuaState {
	public:
		LuaState();
		~LuaState();

		// コピー禁止
		LuaState(const LuaState&) = delete;
		LuaState& operator=(const LuaState&) = delete;

		// ムーブ可能
		LuaState(LuaState&&) noexcept = default;
		LuaState& operator=(LuaState&&) noexcept = default;

		// 初期化
		[[nodiscard]] bool Initialize();

		// スクリプトファイル読み込み・実行
		[[nodiscard]] bool LoadScript(std::string_view scriptPath);
		[[nodiscard]] bool ExecuteString(std::string_view code);

		// ライフサイクル関数呼び出し
		void CallAwake();
		void CallStart();
		void CallUpdate(float deltaTime);
		void CallOnDestroy();

		// プロパティ操作
		[[nodiscard]] std::vector<ScriptProperty> GetPublicProperties() const;
		void SetProperty(std::string_view name, const ScriptPropertyValue& value);
		[[nodiscard]] std::optional<ScriptPropertyValue> GetProperty(std::string_view name) const;

		// エラー取得
		[[nodiscard]] const std::optional<LuaError>& GetLastError() const { return lastError_; }
		void ClearError() { lastError_.reset(); }

		// sol::state直接アクセス（上級者向け）
		[[nodiscard]] sol::state& GetState() { return *lua_; }
		[[nodiscard]] const sol::state& GetState() const { return *lua_; }

		// スクリプトパス取得
		[[nodiscard]] const std::string& GetScriptPath() const { return scriptPath_; }

		// ホットリロード用
		[[nodiscard]] std::filesystem::file_time_type GetLastModifiedTime() const;
		[[nodiscard]] bool CheckAndReload();

	private:
		// エラーハンドリング
		void HandleError(const sol::error& e);
		void HandleError(std::string_view message);

		// 関数が存在するか確認
		[[nodiscard]] bool HasFunction(std::string_view name) const;

		// 安全な関数呼び出し
		template<typename... Args>
		void SafeCall(std::string_view funcName, Args&&... args);

	private:
		std::unique_ptr<sol::state> lua_;
		std::string scriptPath_;
		std::optional<LuaError> lastError_;
		std::filesystem::file_time_type lastModifiedTime_;
		bool isInitialized_ = false;
	};

} // namespace UnoEngine
