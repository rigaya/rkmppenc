// -----------------------------------------------------------------------------------------
//     rkmppenc by rigaya
// -----------------------------------------------------------------------------------------
// The MIT License
//
// Copyright (c) 2014-2017 rigaya
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// IABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// ------------------------------------------------------------------------------------------

#include <vector>
#include <set>
#include <numeric>
#include <filesystem>
#include <cstdio>
#include <cassert>
#include <cstdarg>
#include <signal.h>
#include <thread>
#include "rgy_version.h"
#include "rgy_codepage.h"
#include "rgy_resource.h"
#include "rgy_env.h"
#include "mpp_param.h"
#include "mpp_core.h"
#include "mpp_cmd.h"
#include "rgy_util.h"
#include "rgy_filesystem.h"
#include "rgy_avutil.h"
#include "rgy_opencl.h"
#include "rgy_pipe.h"

static tstring path_to_tstring(const std::filesystem::path& path) {
#if defined(_WIN32) || defined(_WIN64)
    return path.wstring();
#else
    return path.string();
#endif
}

static void cl_perf_print_warn(const TCHAR *format, ...) {
    va_list args;
    va_start(args, format);
    _ftprintf(stderr, _T("cl_perf: warning: "));
#if defined(_WIN32) || defined(_WIN64)
    _vftprintf(stderr, format, args);
#else
    vfprintf(stderr, format, args);
#endif
    va_end(args);
}

static void cl_perf_print_info(const TCHAR *format, ...) {
    va_list args;
    va_start(args, format);
    _ftprintf(stdout, _T("cl_perf: "));
#if defined(_WIN32) || defined(_WIN64)
    _vftprintf(stdout, format, args);
#else
    vfprintf(stdout, format, args);
#endif
    va_end(args);
}

static bool cl_perf_write_resource(const TCHAR *resourceName, const std::filesystem::path& outPath) {
    void *resourceData = nullptr;
    const auto resourceSize = getEmbeddedResource(&resourceData, resourceName, _T("CL_PERF_SRC"), nullptr);
    if (resourceSize <= 0 || resourceData == nullptr) {
        cl_perf_print_warn(_T("failed to load embedded resource %s.\n"), resourceName);
        return false;
    }

    FILE *fp = nullptr;
    const auto outPathT = path_to_tstring(outPath);
    if (_tfopen_s(&fp, outPathT.c_str(), _T("wb")) != 0 || fp == nullptr) {
        cl_perf_print_warn(_T("failed to open %s for writing.\n"), outPathT.c_str());
        return false;
    }
    const auto written = fwrite(resourceData, 1, resourceSize, fp);
    fclose(fp);
    if (written != (size_t)resourceSize) {
        cl_perf_print_warn(_T("failed to write embedded resource %s to %s.\n"), resourceName, outPathT.c_str());
        return false;
    }
    return true;
}

static bool cl_perf_extract_tools(const std::filesystem::path& toolsDir) {
    const auto toolsDirT = path_to_tstring(toolsDir);
    if (!CreateDirectoryRecursive(toolsDirT.c_str())) {
        cl_perf_print_warn(_T("failed to create tool directory %s.\n"), toolsDirT.c_str());
        return false;
    }

    struct ClPerfResource {
        const TCHAR *resourceName;
        const TCHAR *filename;
    };
    static const ClPerfResource resources[] = {
        { _T("CL_PERF_COMMON_PY"),      _T("cl_perf_common.py") },
        { _T("CL_PERF_AGGREGATE_PY"),   _T("cl_perf_aggregate.py") },
        { _T("CL_PERF_REPORT_PY"),      _T("cl_perf_report.py") },
        { _T("CL_PERF_ARCH_TABLE_JSON"), _T("arch_table.json") },
    };

    for (const auto& resource : resources) {
        if (!cl_perf_write_resource(resource.resourceName, toolsDir / resource.filename)) {
            return false;
        }
    }
    return true;
}

static tstring cl_perf_join_args(const std::vector<tstring>& args) {
    tstring joined;
    for (const auto& arg : args) {
        if (!joined.empty()) {
            joined += _T(" ");
        }
        joined += _T("\"") + arg + _T("\"");
    }
    return joined;
}

static void cl_perf_drain_pipe(RGYPipeProcess *process, const bool isStdErr) {
    std::vector<uint8_t> buffer;
    for (;;) {
        buffer.clear();
        const auto ret = isStdErr ? process->stdErrRead(buffer) : process->stdOutRead(buffer);
        if (ret < 0) {
            break;
        }
        if (!buffer.empty()) {
            auto fp = isStdErr ? stderr : stdout;
            fwrite(buffer.data(), 1, buffer.size(), fp);
            fflush(fp);
        }
    }
}

static int cl_perf_run_process(const std::vector<tstring>& args) {
    auto process = createRGYPipeProcess();
    process->init(PIPE_MODE_DISABLE, PIPE_MODE_ENABLE, PIPE_MODE_ENABLE);

    cl_perf_print_info(_T("run %s\n"), cl_perf_join_args(args).c_str());
    if (process->run(args, nullptr, 0, true, false) != 0) {
        cl_perf_print_warn(_T("failed to start %s.\n"), args.empty() ? _T("") : args[0].c_str());
        process->close();
        return -1;
    }

    std::thread stdoutThread(cl_perf_drain_pipe, process.get(), false);
    std::thread stderrThread(cl_perf_drain_pipe, process.get(), true);
    const auto exitCode = process->waitAndGetExitCode();
    stdoutThread.join();
    stderrThread.join();
    process->close();
    return exitCode;
}

static bool cl_perf_check_python(const std::vector<tstring>& pythonArgs) {
    auto args = pythonArgs;
    args.push_back(_T("-c"));
    args.push_back(_T("import sys"));
    return cl_perf_run_process(args) == 0;
}

static int cl_perf_run_python_script(const std::filesystem::path& scriptPath, const std::vector<tstring>& scriptArgs, const tstring& pythonPath) {
    const auto scriptPathT = path_to_tstring(scriptPath);

    std::vector<std::vector<tstring>> pythonArgList;
    if (!pythonPath.empty()) {
        pythonArgList.push_back({ pythonPath });
    } else {
#if defined(_WIN32) || defined(_WIN64)
        pythonArgList.push_back({ _T("py.exe") });
        pythonArgList.push_back({ _T("python.exe") });
#else
        pythonArgList.push_back({ _T("python3") });
#endif
    }

    for (auto pythonArgs : pythonArgList) {
        if (!cl_perf_check_python(pythonArgs)) {
            continue;
        }
        auto args = pythonArgs;
        args.push_back(scriptPathT);
        args.insert(args.end(), scriptArgs.begin(), scriptArgs.end());
        return cl_perf_run_process(args);
    }
    return -1;
}

static void cl_perf_generate_report(const tstring& dumpDir, const tstring& oclocPath, const tstring& pythonPath) {
    if (dumpDir.empty()) {
        return;
    }

    std::error_code ec;
    const auto dumpDirPath = std::filesystem::absolute(std::filesystem::path(dumpDir), ec);
    if (ec || !std::filesystem::is_directory(dumpDirPath, ec)) {
        cl_perf_print_warn(_T("dump directory not found: %s\n"), dumpDir.c_str());
        return;
    }

    const auto toolsDir = dumpDirPath / _T(".cl_perf_tools");
    if (!cl_perf_extract_tools(toolsDir)) {
        return;
    }

    const auto dumpDirT = path_to_tstring(dumpDirPath);
    std::vector<tstring> aggregateArgs = { _T("--dump-dir"), dumpDirT };
    if (!oclocPath.empty()) {
        aggregateArgs.push_back(_T("--ocloc"));
        aggregateArgs.push_back(oclocPath);
    }

    const auto aggregateExitCode = cl_perf_run_python_script(toolsDir / _T("cl_perf_aggregate.py"), aggregateArgs, pythonPath);
    cl_perf_print_info(_T("aggregate exit code: %d\n"), aggregateExitCode);
    if (aggregateExitCode != 0) {
        cl_perf_print_warn(_T("aggregate failed, report generation skipped.\n"));
        return;
    }

    const std::vector<tstring> reportArgs = { _T("--dump-dir"), dumpDirT };
    const auto reportExitCode = cl_perf_run_python_script(toolsDir / _T("cl_perf_report.py"), reportArgs, pythonPath);
    cl_perf_print_info(_T("report exit code: %d\n"), reportExitCode);

    const auto reportPath = dumpDirPath / _T("report.html");
    const auto reportPathT = path_to_tstring(reportPath);
    if (reportExitCode == 0 && std::filesystem::is_regular_file(reportPath, ec)) {
        cl_perf_print_info(_T("report generated: %s\n"), reportPathT.c_str());
    } else {
        cl_perf_print_warn(_T("report.html was not generated: %s\n"), reportPathT.c_str());
    }
}

static void show_version() {
    _ftprintf(stdout, _T("%s\n"), get_encoder_version());
}

static void show_help() {
    _ftprintf(stdout, _T("%s\n"), encoder_help().c_str());
}

static void show_environment_info() {
    _ftprintf(stderr, _T("%s\n"), getEnviromentInfo().c_str());
}

static void show_option_list() {
    show_version();

    std::vector<std::string> optList;
    for (const auto &optHelp : createOptionList()) {
        optList.push_back(optHelp.first);
    }
    std::sort(optList.begin(), optList.end());

    _ftprintf(stdout, _T("Option List:\n"));
    for (const auto &optHelp : optList) {
        _ftprintf(stdout, _T("--%s\n"), char_to_tstring(optHelp).c_str());
    }
}

int parse_print_options(const TCHAR *option_name, const TCHAR *arg1, const RGYParamLogLevel& loglevel) {

#define IS_OPTION(x) (0 == _tcscmp(option_name, _T(x)))

    if (IS_OPTION("help")) {
        show_version();
        show_help();
        return 1;
    }
    if (IS_OPTION("version")) {
        show_version();
        return 1;
    }
    if (IS_OPTION("option-list")) {
        show_option_list();
        return 1;
    }
    if (0 == _tcscmp(option_name, _T("check-hw"))) {
        auto codecs = getMPPEncoderSupport();
        if (codecs.size() == 0) {
            _ftprintf(stdout, _T("Encode not supported!\n"));
            return -1;
        }
        _ftprintf(stdout, _T("Supported Encode Codecs\n"));
        for (auto codec : codecs) {
            _ftprintf(stdout, _T("%s\n"), CodecToStr(codec).c_str());
        }
        return 1;
    }
    if (IS_OPTION("check-environment")) {
        show_environment_info();
        return 1;
    }
    if (0 == _tcscmp(option_name, _T("check-mppinfo"))) {
        _ftprintf(stdout, _T("%s\n"), getMppInfo().c_str());
        return 1;
    }
    if (0 == _tcscmp(option_name, _T("check-rgainfo"))) {
        _ftprintf(stdout, _T("%s\n"), getRGAInfo().c_str());
        return 1;
    }
    if (0 == _tcscmp(option_name, _T("check-clinfo"))) {
        tstring str = getOpenCLInfo(CL_DEVICE_TYPE_GPU);
        _ftprintf(stdout, _T("%s\n"), str.c_str());
        return 1;
    }
#if ENABLE_AVSW_READER
    if (0 == _tcscmp(option_name, _T("check-avcodec-dll"))) {
        const auto ret = check_avcodec_dll();
        _ftprintf(stdout, _T("%s\n"), ret ? _T("yes") : _T("no"));
        if (!ret) {
            _ftprintf(stdout, _T("%s\n"), error_mes_avcodec_dll_not_found().c_str());
        }
        return ret ? 1 : -1;
    }
    if (0 == _tcscmp(option_name, _T("check-avversion"))) {
        _ftprintf(stdout, _T("%s\n"), getAVVersions().c_str());
        return 1;
    }
    if (0 == _tcscmp(option_name, _T("check-codecs"))) {
        _ftprintf(stdout, _T("Video\n"));
        _ftprintf(stdout, _T("%s\n"), getAVCodecs((RGYAVCodecType)(RGY_AVCODEC_DEC), { AVMEDIA_TYPE_VIDEO }).c_str());
        _ftprintf(stdout, _T("\nAudio\n"));
        _ftprintf(stdout, _T("%s\n"), getAVCodecs((RGYAVCodecType)(RGY_AVCODEC_DEC | RGY_AVCODEC_ENC), { AVMEDIA_TYPE_AUDIO }).c_str());
        _ftprintf(stdout, _T("\nSbutitles\n"));
        _ftprintf(stdout, _T("%s\n"), getAVCodecs((RGYAVCodecType)(RGY_AVCODEC_DEC | RGY_AVCODEC_ENC), { AVMEDIA_TYPE_SUBTITLE }).c_str());
        _ftprintf(stdout, _T("\nData / Attachment\n"));
        _ftprintf(stdout, _T("%s\n"), getAVCodecs((RGYAVCodecType)(RGY_AVCODEC_DEC | RGY_AVCODEC_ENC), { AVMEDIA_TYPE_DATA, AVMEDIA_TYPE_ATTACHMENT }).c_str());
        return 1;
    }
    if (0 == _tcscmp(option_name, _T("check-encoders"))) {
        _ftprintf(stdout, _T("Audio\n"));
        _ftprintf(stdout, _T("%s\n"), getAVCodecs((RGYAVCodecType)(RGY_AVCODEC_ENC), { AVMEDIA_TYPE_AUDIO }).c_str());
        _ftprintf(stdout, _T("\nSbutitles\n"));
        _ftprintf(stdout, _T("%s\n"), getAVCodecs((RGYAVCodecType)(RGY_AVCODEC_ENC), { AVMEDIA_TYPE_SUBTITLE }).c_str());
        _ftprintf(stdout, _T("\nData / Attachment\n"));
        _ftprintf(stdout, _T("%s\n"), getAVCodecs((RGYAVCodecType)(RGY_AVCODEC_ENC), { AVMEDIA_TYPE_DATA, AVMEDIA_TYPE_ATTACHMENT }).c_str());
        return 1;
    }
    if (0 == _tcscmp(option_name, _T("check-decoders"))) {
        _ftprintf(stdout, _T("Video\n"));
        _ftprintf(stdout, _T("%s\n"), getAVCodecs((RGYAVCodecType)(RGY_AVCODEC_DEC), { AVMEDIA_TYPE_VIDEO }).c_str());
        _ftprintf(stdout, _T("\nAudio\n"));
        _ftprintf(stdout, _T("%s\n"), getAVCodecs((RGYAVCodecType)(RGY_AVCODEC_DEC), { AVMEDIA_TYPE_AUDIO }).c_str());
        _ftprintf(stdout, _T("\nSbutitles\n"));
        _ftprintf(stdout, _T("%s\n"), getAVCodecs((RGYAVCodecType)(RGY_AVCODEC_DEC), { AVMEDIA_TYPE_SUBTITLE }).c_str());
        _ftprintf(stdout, _T("\nData / Attachment\n"));
        _ftprintf(stdout, _T("%s\n"), getAVCodecs((RGYAVCodecType)(RGY_AVCODEC_DEC), { AVMEDIA_TYPE_DATA, AVMEDIA_TYPE_ATTACHMENT }).c_str());
        return 1;
    }
    if (0 == _tcscmp(option_name, _T("check-profiles"))) {
        auto list = getAudioPofileList(arg1);
        if (list.size() == 0) {
            _ftprintf(stdout, _T("Failed to find codec name \"%s\"\n"), arg1);
        } else {
            _ftprintf(stdout, _T("profile name for \"%s\"\n"), arg1);
            for (const auto &name : list) {
                _ftprintf(stdout, _T("  %s\n"), name.c_str());
            }
        }
        return 1;
    }
    if (0 == _tcscmp(option_name, _T("check-protocols"))) {
        _ftprintf(stdout, _T("%s\n"), getAVProtocols().c_str());
        return 1;
    }
    if (0 == _tcscmp(option_name, _T("check-formats"))) {
        _ftprintf(stdout, _T("%s\n"), getAVFormats((RGYAVFormatType)(RGY_AVFORMAT_DEMUX | RGY_AVFORMAT_MUX)).c_str());
        return 1;
    }
    if (0 == _tcscmp(option_name, _T("check-avdevices"))) {
        _ftprintf(stdout, _T("%s\n"), getAVDevices().c_str());
        return 1;
    }
    if (0 == _tcscmp(option_name, _T("check-filters"))) {
        _ftprintf(stdout, _T("%s\n"), getAVFilters().c_str());
        return 1;
    }
#endif //#if ENABLE_AVSW_READER
#undef IS_OPTION
    return 0;
}

//Ctrl + C ハンドラ
static bool g_signal_abort = false;
#pragma warning(push)
#pragma warning(disable:4100)
static void sigcatch(int sig) {
    g_signal_abort = true;
}
#pragma warning(pop)
static int set_signal_handler() {
    int ret = 0;
    if (SIG_ERR == signal(SIGINT, sigcatch)) {
        _ftprintf(stderr, _T("failed to set signal handler.\n"));
    }
    return ret;
}

#if defined(_WIN32) || defined(_WIN64)
static bool check_locale_is_ja() {
    const WORD LangID_ja_JP = MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);
    return GetUserDefaultLangID() == LangID_ja_JP;
}

static tstring getErrorFmtStr(uint32_t err) {
    TCHAR errmes[4097];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, NULL, errmes, _countof(errmes), NULL);
    return errmes;
}

static int run_on_os_codepage() {
    auto exepath = getExePath();
    auto tmpexe = std::filesystem::path(PathRemoveExtensionS(exepath));
    tmpexe += strsprintf(_T("A_%x"), GetCurrentProcessId());
    tmpexe += std::filesystem::path(exepath).extension();
    std::filesystem::copy_file(exepath, tmpexe, std::filesystem::copy_options::overwrite_existing);

    SetLastError(0);
    HANDLE handle = BeginUpdateResourceW(tmpexe.wstring().c_str(), FALSE);
    if (handle == NULL) {
        auto lasterr = GetLastError();
        _ftprintf(stderr, _T("Failed to create temporary exe file: [%d] %s.\n"), lasterr, getErrorFmtStr(lasterr).c_str());
        return 1;
    }
    void *manifest = nullptr;
    int size = getEmbeddedResource(&manifest,_T("APP_OSCODEPAGE_MANIFEST"), _T("EXE_DATA"), NULL);
    if (size == 0) {
        _ftprintf(stderr, _T("Failed to load manifest for OS codepage mode.\n"));
        return 1;
    }
    SetLastError(0);
    if (!UpdateResourceW(handle, RT_MANIFEST, CREATEPROCESS_MANIFEST_RESOURCE_ID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), manifest, size)) {
        auto lasterr = GetLastError();
        _ftprintf(stderr, _T("Failed to update manifest for ansi mode: [%d] %s.\n"), lasterr, getErrorFmtStr(lasterr).c_str());
        return 1;
    }
    SetLastError(0);
    if (!EndUpdateResourceW(handle, FALSE)) {
        auto lasterr = GetLastError();
        _ftprintf(stderr, _T("Failed to finish update manifest for OS codepage mode: [%d] %s.\n"), lasterr, getErrorFmtStr(lasterr).c_str());
        return 1;
    }

    const auto commandline = str_replace(str_replace(GetCommandLineW(),
        std::filesystem::path(exepath).filename(), std::filesystem::path(tmpexe).filename()),
        CODEPAGE_CMDARG, CODEPAGE_CMDARG_APPLIED);

    int ret = 0;
    try {
        DWORD flags = 0; // CREATE_NO_WINDOW;

        HANDLE hStdIn, hStdOut, hStdErr;
        DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_INPUT_HANDLE),  GetCurrentProcess(), &hStdIn,  0, TRUE, DUPLICATE_SAME_ACCESS);
        DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_OUTPUT_HANDLE), GetCurrentProcess(), &hStdOut, 0, TRUE, DUPLICATE_SAME_ACCESS);
        DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_ERROR_HANDLE),  GetCurrentProcess(), &hStdErr, 0, TRUE, DUPLICATE_SAME_ACCESS);

        SECURITY_ATTRIBUTES sa;
        memset(&sa, 0, sizeof(SECURITY_ATTRIBUTES));
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE; //TRUEでハンドルを引き継ぐ

        STARTUPINFO si;
        memset(&si, 0, sizeof(STARTUPINFO));
        si.cb = sizeof(STARTUPINFO);
        //si.dwFlags |= STARTF_USESHOWWINDOW;
        si.dwFlags |= STARTF_USESTDHANDLES;
        //si.wShowWindow |= SW_SHOWMINNOACTIVE;
        si.hStdInput = hStdIn;
        si.hStdOutput = hStdOut;
        si.hStdError = hStdErr;

        PROCESS_INFORMATION pi;
        memset(&pi, 0, sizeof(PROCESS_INFORMATION));

        SetLastError(0);
        if (CreateProcess(nullptr, (LPWSTR)commandline.c_str(), &sa, nullptr, TRUE, flags, nullptr, nullptr, &si, &pi) == 0) {
            auto lasterr = GetLastError();
            _ftprintf(stderr, _T("Failed to run process in OS codepage mode: [%d] %s.\n"), lasterr, getErrorFmtStr(lasterr).c_str());
            ret = 1;
        } else {
            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD proc_ret = 0;
            if (GetExitCodeProcess(pi.hProcess, &proc_ret)) {
                ret = (int)proc_ret;
            }
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    } catch (...) {
        ret = 1;
    }
    std::filesystem::remove(tmpexe);
    return ret;
}
#endif //#if defined(_WIN32) || defined(_WIN64)

int mpp_run(MPPParam *pParams) {
    const auto clPerfDumpDir = pParams->ctrl.clPerfDumpDir;
    const auto clPerfOclocPath = pParams->ctrl.clPerfOclocPath;
    const auto pythonPath = pParams->ctrl.pythonPath;
    const bool clPerfGenerateReport = !pParams->ctrl.parallelEnc.isChild();
    auto mpp = std::make_unique<MPPCore>();
    if (mpp->init(pParams) != RGY_ERR_NONE) {
        return 1;
    }
    mpp->PrintEncoderParam();
    mpp->SetAbortFlagPointer(&g_signal_abort);
    set_signal_handler();

    try {
        if (mpp->run2() != RGY_ERR_NONE) {
            mpp.reset();
            if (clPerfGenerateReport) {
                cl_perf_generate_report(clPerfDumpDir, clPerfOclocPath, pythonPath);
            }
            return 1;
        }
    } catch (...) {
        _ftprintf(stderr, _T("fatal error in encoding pipeline.\n"));
        mpp.reset();
        if (clPerfGenerateReport) {
            cl_perf_generate_report(clPerfDumpDir, clPerfOclocPath, pythonPath);
        }
        return 1;
    }
    mpp.reset();
    if (clPerfGenerateReport) {
        cl_perf_generate_report(clPerfDumpDir, clPerfOclocPath, pythonPath);
    }
    return 0;
}

int _tmain(int argc, TCHAR **argv) {
#if defined(_WIN32) || defined(_WIN64)
    if (check_locale_is_ja()) {
        _tsetlocale(LC_ALL, _T("Japanese"));
    }
#endif //#if defined(_WIN32) || defined(_WIN64)

    if (argc == 1) {
        show_version();
        show_help();
        return 1;
    }

#if defined(_WIN32) || defined(_WIN64)
    if (GetACP() == CODE_PAGE_UTF8) {
        bool switch_to_os_cp = false;
        for (int iarg = 1; iarg < argc; iarg++) {
            if (iarg + 1 < argc
                && _tcscmp(argv[iarg + 0], CODEPAGE_CMDARG) == 0) {
                if (_tcscmp(argv[iarg + 1], _T("os")) == 0) {
                    switch_to_os_cp = true;
                } else if (_tcscmp(argv[iarg + 1], _T("utf8")) == 0) {
                    switch_to_os_cp = false;
                } else {
                    _ftprintf(stderr, _T("Unknown option for %s.\n"), CODEPAGE_CMDARG);
                    return 1;
                }
            }
        }
        if (switch_to_os_cp) {
            return run_on_os_codepage();
        }
    }
#endif //#if defined(_WIN32) || defined(_WIN64)

    RGYParamLogLevel loglevelPrint(RGY_LOG_ERROR);
    for (int iarg = 1; iarg < argc-1; iarg++) {
        if (tstring(argv[iarg]) == _T("--log-level")) {
            parse_log_level_param(argv[iarg], argv[iarg+1], loglevelPrint);
            break;
        }
    }

    for (int iarg = 1; iarg < argc; iarg++) {
        const TCHAR *option_name = nullptr;
        if (argv[iarg][0] == _T('-')) {
            if (argv[iarg][1] == _T('\0')) {
                continue;
            } else if (argv[iarg][1] == _T('-')) {
                option_name = &argv[iarg][2];
            } else if (argv[iarg][2] == _T('\0')) {
                if (nullptr == (option_name = cmd_short_opt_to_long(argv[iarg][1]))) {
                    continue;
                }
            }
        }
        if (option_name != nullptr) {
            int ret = parse_print_options(option_name, (iarg+1 < argc) ? argv[iarg+1] : _T(""), loglevelPrint);
            if (ret != 0) {
                return ret == 1 ? 0 : 1;
            }
        }
    }

    //optionファイルの読み取り
    std::vector<tstring> argvCnfFile;
    for (int iarg = 1; iarg < argc; iarg++) {
        const TCHAR *option_name = nullptr;
        if (argv[iarg][0] == _T('-')) {
            if (argv[iarg][1] == _T('\0')) {
                continue;
            } else if (argv[iarg][1] == _T('-')) {
                option_name = &argv[iarg][2];
            }
        }
        if (option_name != nullptr
            && tstring(option_name) == _T("option-file")) {
            if (iarg + 1 >= argc) {
                _ftprintf(stderr, _T("option file name is not specified.\n"));
                return -1;
            }
            tstring cnffile = argv[iarg + 1];
            vector_cat(argvCnfFile, cmd_from_config_file(argv[iarg + 1]));
        }
    }

    MPPParam prm;
    std::vector<const TCHAR *> argvCopy(argv, argv + argc);
    //optionファイルのパラメータを追加
    for (size_t i = 0; i < argvCnfFile.size(); i++) {
        if (argvCnfFile[i].length() > 0) {
            argvCopy.push_back(argvCnfFile[i].c_str());
        }
    }
    argvCopy.push_back(_T(""));
    if (parse_cmd(&prm, (int)argvCopy.size()-1, argvCopy.data())) {
        return 1;
    }

    if (prm.common.inputFilename != _T("-")
        && prm.common.outputFilename != _T("-")
        && rgy_path_is_same(prm.common.inputFilename, prm.common.outputFilename)) {
        _ftprintf(stderr, _T("destination file is equal to source file!"));
        return 1;
    }

    if (mpp_run(&prm)) {
        fprintf(stderr, "Finished with error in rkmppenc.\n");
        return 1;
    }
    return 0;
}
