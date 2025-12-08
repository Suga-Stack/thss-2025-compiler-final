#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Minimal type system for SysY -> LLVM IR framework
class Type {
public:
	enum TypeID { IntTy, VoidTy, PointerTy, ArrayTy, FunctionTy };
	explicit Type(TypeID id) : id(id) {}
	virtual ~Type() = default;
	TypeID getID() const { return id; }
	virtual std::string toString() const = 0;

private:
	TypeID id;
};

using TypePtr = std::shared_ptr<Type>;

// Int type (singleton for simplicity)
class IntType : public Type {
public:
	static TypePtr get() {
		static TypePtr instance = std::make_shared<IntType>();
		return instance;
	}
	std::string toString() const override { return "i32"; }
	IntType() : Type(Type::IntTy) {}
};

class VoidType : public Type {
public:
	static TypePtr get() {
		static TypePtr instance = std::make_shared<VoidType>();
		return instance;
	}
	std::string toString() const override { return "void"; }
	VoidType() : Type(Type::VoidTy) {}
};

class PointerType : public Type {
public:
	explicit PointerType(TypePtr pointee) : Type(Type::PointerTy), pointee(pointee) {}
	TypePtr getPointee() const { return pointee; }
	std::string toString() const override { return pointee->toString() + "*"; }
private:
	TypePtr pointee;
};

class ArrayType : public Type {
public:
	ArrayType(TypePtr elementType, uint64_t count)
			: Type(Type::ArrayTy), elementType(elementType), elementCount(count) {}
	TypePtr getElementType() const { return elementType; }
	uint64_t getElementCount() const { return elementCount; }
	std::string toString() const override {
		return "[" + std::to_string(elementCount) + " x " + elementType->toString() + "]";
	}
private:
	TypePtr elementType;
	uint64_t elementCount;
};

class FunctionType : public Type {
public:
	FunctionType(TypePtr ret, const std::vector<TypePtr> &args)
			: Type(Type::FunctionTy), ret(ret), args(args) {}
	TypePtr getReturnType() const { return ret; }
	const std::vector<TypePtr> &getParamTypes() const { return args; }
	std::string toString() const override {
		std::string s = ret->toString() + " (";
		for (size_t i = 0; i < args.size(); ++i) {
			s += args[i]->toString();
			if (i + 1 < args.size()) s += ", ";
		}
		s += ")";
		return s;
	}
private:
	TypePtr ret;
	std::vector<TypePtr> args;
};
