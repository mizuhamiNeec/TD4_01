#pragma once
#include <memory>
#include <vector>

#include "World.h"

#include "engine/Entity/Entity.h"

namespace Unnamed::ECS {
	class World;

	class ICommand {
	public:
		virtual      ~ICommand() = default;
		virtual void Apply(World& world) = 0;
	};

	class CommandBuffer {
	public:
		CommandBuffer() = default;

		void Clear() { mCommands.clear(); }
		bool Empty() const { return mCommands.empty(); }

		void Apply(World& world);

		void Destroy(Entity entity);

		template <typename T>
		void Add(Entity entity, T value = {});

		template <typename T>
		void Remove(Entity entity);

	private:
		std::vector<std::unique_ptr<ICommand>> mCommands;
	};

	class DestroyCommand final : public ICommand {
	public :
		explicit DestroyCommand(const Entity entity) : mEntity(entity) {
		}

		void Apply(World& world) override { world.DestroyImmediate(mEntity); }

	private:
		Entity mEntity;
	};

	template <typename T>
	class AddCommand final : public ICommand {
	public:
		AddCommand(const Entity entity, T value) : mEntity(entity),
		                                           mValue(std::move(value)) {
		}

		void Apply(World& world) override {
			if (world.IsAlive(mEntity)) {
				world.AddImmediate<T>(mEntity, std::move(mValue));
			}
		}

	private:
		Entity mEntity;
		T      mValue;
	};

	template <typename T>
	class RemoveCommand final : public ICommand {
	public:
		explicit RemoveCommand(const Entity entity) : mEntity(entity) {
		}

		void Apply(World& world) override {
			if (world.IsAlive(mEntity)) {
				world.RemoveImmediate<T>(mEntity);
			}
		}

	private:
		Entity mEntity;
	};

	inline void CommandBuffer::Destroy(Entity entity) {
		mCommands.emplace_back(
			std::make_unique<DestroyCommand>(entity)
		);
	}

	template <typename T>
	void CommandBuffer::Add(Entity entity, T value) {
		mCommands.emplace_back(
			std::make_unique<AddCommand<T>>(entity, std::move(value))
		);
	}

	template <typename T>
	void CommandBuffer::Remove(Entity entity) {
		mCommands.emplace_back(std::make_unique<RemoveCommand<T>>(entity));
	}
}
