
#include "command_line.h"

#include <windows.h>

#include <algorithm>

#include "file_path.h"
#include "logging.h"
#include "string_split.h"
#include "string_util.h"
#include "utf_string_conversions.h"

namespace
{

    typedef base::CommandLine::StringType::value_type CharType;

    // ��Ϊ����lazyƥ��, ����ǰ׺����(L"--")������ڶ̵�(L"-")ǰ��.
    const CharType kSwitchTerminator[] = L"--";
    const CharType kSwitchValueSeparator[] = L"=";
    const CharType* const kSwitchPrefixes[] = { L"--", L"-", L"/" };

    // Lowercase�ַ���. ʹ��lowercaseת���ַ�.
    void Lowercase(std::string* arg)
    {
        transform(arg->begin(), arg->end(), arg->begin(), tolower);
    }

    // ��Ҫʱ������������, ����CommandLineToArgvW()�ͻ����������һ������.
    std::wstring WindowsStyleQuote(const std::wstring& arg)
    {
        // ����CommandLineToArgvW�����Ź���.
        // http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
        if(arg.find_first_of(L" \\\"") == std::wstring::npos)
        {
            // û���������ű�Ҫ.
            return arg;
        }

        std::wstring out;
        out.push_back(L'"');
        for(size_t i=0; i<arg.size(); ++i)
        {
            if(arg[i] == '\\')
            {
                // ���ҷ�б������
                size_t start = i, end = start + 1;
                for(; end<arg.size()&&arg[end]=='\\'; ++end)
                {
                    /* empty */;
                }
                size_t backslash_count = end - start;

                // �������"�ķ�б�ܻᱻת��. ��Ϊ�����ַ�����β����һ��",
                // ������"�Ż��߽�β������ת�巴����.
                if(end==arg.size() || arg[end]=='"')
                {
                    // ��Ҫ���2�������ķ�б��.
                    backslash_count *= 2;
                }
                for(size_t j=0; j<backslash_count; ++j)
                {
                    out.push_back('\\');
                }

                // ����iλ��(ע��ѭ���е�++i)
                i = end - 1;
            }
            else if(arg[i] == '"')
            {
                out.push_back('\\');
                out.push_back('"');
            }
            else
            {
                out.push_back(arg[i]);
            }
        }
        out.push_back('"');

        return out;
    }

    // ����true�����|switch_string|��|switch_value|���parameter_string
    // ��ʾһ������.
    bool IsSwitch(const base::CommandLine::StringType& parameter_string,
        std::string* switch_string, base::CommandLine::StringType* switch_value)
    {
        switch_string->clear();
        switch_value->clear();

        for(size_t i=0; i<arraysize(kSwitchPrefixes); ++i)
        {
            base::CommandLine::StringType prefix(kSwitchPrefixes[i]);
            if(parameter_string.find(prefix) != 0)
            {
                continue;
            }

            const size_t switch_start = prefix.length();
            const size_t equals_position = parameter_string.find(
                kSwitchValueSeparator, switch_start);
            base::CommandLine::StringType switch_native;
            if(equals_position == base::CommandLine::StringType::npos)
            {
                switch_native = parameter_string.substr(switch_start);
            }
            else
            {
                switch_native = parameter_string.substr(
                    switch_start, equals_position-switch_start);
                *switch_value = parameter_string.substr(equals_position+1);
            }
            *switch_string = WideToASCII(switch_native);
            Lowercase(switch_string);

            return true;
        }

        return false;
    }

}

namespace base
{

    CommandLine* CommandLine::current_process_commandline_ = NULL;

    CommandLine::CommandLine(NoProgram no_program) {}

    CommandLine::CommandLine(const FilePath& program)
    {
        if(!program.empty())
        {
            program_ = program.value();
            // TODO: ������Ҫ��������.
            command_line_string_ = L'"' + program.value() + L'"';
        }
    }

    CommandLine::~CommandLine() {}

    void CommandLine::Init(int argc, const char* const* argv)
    {
        delete current_process_commandline_;
        current_process_commandline_ = new CommandLine;
        current_process_commandline_->ParseFromString(::GetCommandLineW());
    }

    void CommandLine::Reset()
    {
        DCHECK(current_process_commandline_ != NULL);
        delete current_process_commandline_;
        current_process_commandline_ = NULL;
    }

    CommandLine* CommandLine::ForCurrentProcess()
    {
        DCHECK(current_process_commandline_);
        return current_process_commandline_;
    }

    CommandLine CommandLine::FromString(const std::wstring& command_line)
    {
        CommandLine cmd;
        cmd.ParseFromString(command_line);
        return cmd;
    }

    CommandLine::StringType CommandLine::command_line_string() const
    {
        return command_line_string_;
    }

    FilePath CommandLine::GetProgram() const
    {
        return FilePath(program_);
    }

    bool CommandLine::HasSwitch(const std::string& switch_string) const
    {
        std::string lowercased_switch(switch_string);
        Lowercase(&lowercased_switch);
        return switches_.find(lowercased_switch)!=switches_.end();
    }

    std::string CommandLine::GetSwitchValueASCII(
        const std::string& switch_string) const
    {
        CommandLine::StringType value = GetSwitchValueNative(switch_string);
        if(!IsStringASCII(value))
        {
            LOG(WARNING) << "Value of --" << switch_string << " must be ASCII.";
            return "";
        }
        return WideToASCII(value);
    }

    FilePath CommandLine::GetSwitchValuePath(
        const std::string& switch_string) const
    {
        return FilePath(GetSwitchValueNative(switch_string));
    }

    CommandLine::StringType CommandLine::GetSwitchValueNative(
        const std::string& switch_string) const
    {
        std::string lowercased_switch(switch_string);
        Lowercase(&lowercased_switch);

        SwitchMap::const_iterator result = switches_.find(lowercased_switch);

        if(result == switches_.end())
        {
            return CommandLine::StringType();
        }
        else
        {
            return result->second;
        }
    }

    void CommandLine::AppendSwitch(const std::string& switch_string)
    {
        command_line_string_.append(L" ");
        command_line_string_.append(kSwitchPrefixes[0] +
            ASCIIToWide(switch_string));
        switches_[switch_string] = L"";
    }

    void CommandLine::AppendSwitchPath(const std::string& switch_string,
        const FilePath& path)
    {
        AppendSwitchNative(switch_string, path.value());
    }

    void CommandLine::AppendSwitchNative(const std::string& switch_string,
        const CommandLine::StringType& value)
    {
        StringType combined_switch_string = kSwitchPrefixes[0] +
            ASCIIToWide(switch_string);
        if(!value.empty())
        {
            combined_switch_string += kSwitchValueSeparator +
                WindowsStyleQuote(value);
        }

        command_line_string_.append(L" ");
        command_line_string_.append(combined_switch_string);

        switches_[switch_string] = value;
    }

    void CommandLine::AppendSwitchASCII(const std::string& switch_string,
        const std::string& value_string)
    {
        AppendSwitchNative(switch_string, ASCIIToWide(value_string));
    }

    void CommandLine::AppendSwitches(const CommandLine& other)
    {
        SwitchMap::const_iterator i;
        for(i=other.switches_.begin(); i!=other.switches_.end(); ++i)
        {
            AppendSwitchNative(i->first, i->second);
        }
    }

    void CommandLine::CopySwitchesFrom(const CommandLine& source,
        const char* const switches[], size_t count)
    {
        for(size_t i=0; i<count; ++i)
        {
            if(source.HasSwitch(switches[i]))
            {
                StringType value = source.GetSwitchValueNative(switches[i]);
                AppendSwitchNative(switches[i], value);
            }
        }
    }

    void CommandLine::AppendArg(const std::string& value)
    {
        DCHECK(IsStringUTF8(value));
        AppendArgNative(UTF8ToWide(value));
    }

    void CommandLine::AppendArgPath(const FilePath& path)
    {
        AppendArgNative(path.value());
    }

    void CommandLine::AppendArgNative(const std::wstring& value)
    {
        command_line_string_.append(L" ");
        command_line_string_.append(WindowsStyleQuote(value));
        args_.push_back(value);
    }

    void CommandLine::AppendArgs(const CommandLine& other)
    {
        if(other.args_.size() <= 0)
        {
            return;
        }
        command_line_string_.append(L" --");
        StringVector::const_iterator i;
        for(i=other.args_.begin(); i!=other.args_.end(); ++i)
        {
            AppendArgNative(*i);
        }
    }

    void CommandLine::AppendArguments(const CommandLine& other,
        bool include_program)
    {
        // ��֤include_program�Ƿ���ȷʹ��.
        DCHECK(!include_program || !other.GetProgram().empty());
        if(include_program)
        {
            program_ = other.program_;
        }

        if(!command_line_string_.empty())
        {
            command_line_string_ += L' ';
        }

        command_line_string_ += other.command_line_string_;

        SwitchMap::const_iterator i;
        for(i=other.switches_.begin(); i!=other.switches_.end(); ++i)
        {
            switches_[i->first] = i->second;
        }
    }

    void CommandLine::PrependWrapper(const CommandLine::StringType& wrapper)
    {
        // wrapper���ܰ�������(��"gdb --args"). �������ǲ����κζ���Ĵ���, ���Կո�
        // �����з�.
        if(wrapper.empty())
        {
            return;
        }
        StringVector wrapper_and_args;
        base::SplitString(wrapper, ' ', &wrapper_and_args);
        program_ = wrapper_and_args[0];
        command_line_string_ = wrapper + L" " + command_line_string_;
    }

    void CommandLine::ParseFromString(const std::wstring& command_line)
    {
        TrimWhitespace(command_line, TRIM_ALL, &command_line_string_);

        if(command_line_string_.empty())
        {
            return;
        }

        int num_args = 0;
        wchar_t** args = NULL;

        args = CommandLineToArgvW(command_line_string_.c_str(), &num_args);

        // ȥ����һ���������˿հײ���䵽_program
        TrimWhitespace(args[0], TRIM_ALL, &program_);

        bool parse_switches = true;
        for(int i=1; i<num_args; ++i)
        {
            std::wstring arg;
            TrimWhitespace(args[i], TRIM_ALL, &arg);

            if(!parse_switches)
            {
                args_.push_back(arg);
                continue;
            }

            if(arg == kSwitchTerminator)
            {
                parse_switches = false;
                continue;
            }

            std::string switch_string;
            std::wstring switch_value;
            if(IsSwitch(arg, &switch_string, &switch_value))
            {
                switches_[switch_string] = switch_value;
            }
            else
            {
                args_.push_back(arg);
            }
        }

        if(args)
        {
            LocalFree(args);
        }
    }

    CommandLine::CommandLine() {}

} //namespace base