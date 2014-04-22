#include "stdafx.h"
#include "ExecutedAddressManager.hpp"

#include <unordered_map>
#include <set>

#include "tools/Log.hpp"

#include "CppCoverageException.hpp"
#include "ModuleCoverage.hpp"
#include "FileCoverage.hpp"

namespace CppCoverage
{
	//-------------------------------------------------------------------------
	struct ExecutedAddressManager::Instruction
	{
		explicit Instruction(unsigned char instruction)
			: instruction_(instruction)
			, hasBeenExecuted_(false)
		{
		}

		unsigned char instruction_;
		bool hasBeenExecuted_;
	};
	
	//-------------------------------------------------------------------------
	struct ExecutedAddressManager::Line
	{
		bool HasLineBeenExecuted() const
		{
			for (const auto& instruction : instructions_)
				if (instruction.hasBeenExecuted_)
					return true;
			return false;
		}

		// We cannot use vector because we will take the adress of the item
		std::list<ExecutedAddressManager::Instruction> instructions_;
	};

	//-------------------------------------------------------------------------
	struct ExecutedAddressManager::File
	{		
		// We cannot use set because Line can be updated by MarkAddressAsExecuted
		std::map<unsigned int, Line> lines;
	};

	//-------------------------------------------------------------------------
	struct ExecutedAddressManager::Module
	{
		explicit Module(const std::wstring& name)
		: name_(name)
		{
		}

		std::wstring name_;
		std::unordered_map<std::wstring, File> files_;
	};
	
	//-------------------------------------------------------------------------
	ExecutedAddressManager::ExecutedAddressManager()
	{
	}
	
	//-------------------------------------------------------------------------
	ExecutedAddressManager::~ExecutedAddressManager()
	{
	}

	//-------------------------------------------------------------------------
	void ExecutedAddressManager::SetCurrentModule(const std::wstring& moduleName)
	{
		modules_.push_back(std::unique_ptr<Module>(new Module(moduleName)));
	}
	
	//-------------------------------------------------------------------------
	void ExecutedAddressManager::RegisterAddress(
		void* address, 
		const std::wstring& filename, 
		unsigned int lineNumber, 
		unsigned char instructionValue)
	{
		auto& module = GetCurrentModule();
		auto& file = module.files_[filename];
		auto& lines = file.lines;
		auto& line = lines[lineNumber];

		LOG_TRACE << "RegisterAddress: " << address << " for " << filename << ":" << lineNumber;

		line.instructions_.push_back(Instruction{instructionValue});
		auto* instruction = &line.instructions_.back();
		
		if (!addressLineMap_.emplace(address, instruction).second)
			THROW("Address already register");
	}

	//-------------------------------------------------------------------------
	ExecutedAddressManager::Module& ExecutedAddressManager::GetCurrentModule()
	{
		if (modules_.empty())
			THROW("Not current module set");
		return *modules_.back();
	}

	//-------------------------------------------------------------------------
	unsigned char ExecutedAddressManager::MarkAddressAsExecuted(void* address)
	{
		auto it = addressLineMap_.find(address);

		if (it == addressLineMap_.end())
			THROW("Address should be register first");

		Instruction* instruction = it->second;

		if (!instruction)
			THROW("Line cannot be null");

		instruction->hasBeenExecuted_ = true;
			 
		return instruction->instruction_;
	}
	// $$ add pdb in verbose mode 
	//-------------------------------------------------------------------------
	CoverageData ExecutedAddressManager::CreateCoverageData(const std::wstring& name) const
	{
		CoverageData coverageData{name};
		
		for (const auto& module : modules_)
		{
			LOG_INFO << L"Create coverage report for " << module->name_;
			auto& moduleCoverage = coverageData.AddModule(module->name_);

			for (const auto& file : module->files_)
			{
				const std::wstring& name = file.first;
				const File& fileData = file.second;

				auto& fileCoverage = moduleCoverage.AddFile(name);

				for (const auto& pair : fileData.lines)
				{
					auto lineNumber = pair.first;
					const auto& line = pair.second;
					
					fileCoverage.AddLine(lineNumber, line.HasLineBeenExecuted());
				}
			}			
		}

		coverageData.ComputeCoverageRate();
		return coverageData;
	}
}
