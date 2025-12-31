#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// 最小化类型系统，用于 SysY 到 LLVM IR 的转换框架
class Type
{
public:
	// 类型ID枚举
	enum TypeID
	{
		IntTy,		// 32位整数 (i32)
		VoidTy,		// Void类型 (void)
		PointerTy,	// 指针类型 (type*)
		ArrayTy,	// 数组类型 ([n x type])
		FunctionTy, // 函数类型
		BoolTy		// 布尔类型 (i1)
	};
	explicit Type(TypeID id) : id(id) {}
	virtual ~Type() = default;
	TypeID getID() const { return id; }
	// 返回类型的字符串表示 (用于生成 IR)
	virtual std::string toString() const = 0;

private:
	TypeID id;
};

using TypePtr = std::shared_ptr<Type>;

// 整数类型 (单例模式，简化管理)
class IntType : public Type
{
public:
	static TypePtr get()
	{
		static TypePtr instance = std::make_shared<IntType>();
		return instance;
	}
	std::string toString() const override { return "i32"; }
	IntType() : Type(Type::IntTy) {}
};

// Void 类型
class VoidType : public Type
{
public:
	static TypePtr get()
	{
		static TypePtr instance = std::make_shared<VoidType>();
		return instance;
	}
	std::string toString() const override { return "void"; }
	VoidType() : Type(Type::VoidTy) {}
};

// 指针类型
class PointerType : public Type
{
public:
	explicit PointerType(TypePtr pointee) : Type(Type::PointerTy), pointee(pointee) {}
	TypePtr getPointee() const { return pointee; }
	std::string toString() const override { return pointee->toString() + "*"; }

private:
	TypePtr pointee; // 指向的类型
};

// 数组类型
class ArrayType : public Type
{
public:
	ArrayType(TypePtr elementType, uint64_t count)
		: Type(Type::ArrayTy), elementType(elementType), elementCount(count) {}
	TypePtr getElementType() const { return elementType; }
	uint64_t getElementCount() const { return elementCount; }
	std::string toString() const override
	{
		return "[" + std::to_string(elementCount) + " x " + elementType->toString() + "]";
	}

private:
	TypePtr elementType;   // 元素类型
	uint64_t elementCount; // 元素数量
};

// 函数类型
class FunctionType : public Type
{
public:
	FunctionType(TypePtr ret, const std::vector<TypePtr> &args)
		: Type(Type::FunctionTy), ret(ret), args(args) {}
	TypePtr getReturnType() const { return ret; }
	const std::vector<TypePtr> &getParamTypes() const { return args; }
	std::string toString() const override
	{
		std::string s = ret->toString() + " (";
		for (size_t i = 0; i < args.size(); ++i)
		{
			s += args[i]->toString();
			if (i + 1 < args.size())
				s += ", ";
		}
		s += ")";
		return s;
	}

private:
	TypePtr ret;			   // 返回值类型
	std::vector<TypePtr> args; // 参数类型列表
};

// 布尔类型 (i1)
class BoolType : public Type
{
public:
	static TypePtr get()
	{
		static TypePtr instance = std::make_shared<BoolType>();
		return instance;
	}
	std::string toString() const override { return "i1"; }
	BoolType() : Type(Type::BoolTy) {}
};
