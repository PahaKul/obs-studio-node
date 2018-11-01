#include "nodeobs_service.h"
#include <ShlObj.h>
#include <windows.h>
#include "error.hpp"
#include "shared.hpp"

namespace std
{
	template<>
	struct default_delete<obs_output_t> {
		void operator()(obs_output_t* ptr) {
			if (!obs_initialized())
				throw "Trying to delete an obs object but the service isn't active";
			obs_output_release(ptr);
		}
	};

	template<>
	struct default_delete<obs_encoder_t> {
		void operator()(obs_encoder_t* ptr) {
			if (!obs_initialized())
				throw "Trying to delete an obs object but the service isn't active";
			obs_encoder_release(ptr);
		}
	};

	template<>
	struct default_delete<obs_service_t> {
		void operator()(obs_service_t* ptr) {
			if (!obs_initialized())
				throw "Trying to delete an obs object but the service isn't active";
			obs_service_release(ptr);
		}
	};
} // namespace std

std::shared_ptr<obs_output_t>   s_streamingOutput;
std::shared_ptr<obs_output_t>   s_recordingOutput;
std::shared_ptr<obs_encoder_t>  s_audioStreamingEncoder;
std::shared_ptr<obs_encoder_t>  s_audioRecordingEncoder;
std::shared_ptr<obs_encoder_t>	s_videoStreamingEncoder;
std::shared_ptr<obs_encoder_t>	s_videoRecordingEncoder;
std::shared_ptr<obs_service_t>  s_service;

std::string s_aacRecEncID;
std::string s_aacStreamEncID;

std::string s_videoEncoder;
std::string s_videoQuality;
bool        s_usingRecordingPreset = false;
bool        s_recordingConfigured  = false;
bool        s_ffmpegOutput         = false;
bool        s_lowCPUx264           = false;
bool        s_isStreaming          = false;
bool        s_isRecording          = false;

OBS_service::OBS_service() {}
OBS_service::~OBS_service() {}

void OBS_service::Register(ipc::server& srv)
{
	std::shared_ptr<ipc::collection> cls = std::make_shared<ipc::collection>("Service");

	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_resetAudioContext", std::vector<ipc::type>{}, OBS_service_resetAudioContext));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_resetVideoContext", std::vector<ipc::type>{}, OBS_service_resetVideoContext));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_createAudioEncoder", std::vector<ipc::type>{}, OBS_service_createAudioEncoder));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_createVideoStreamingEncoder", std::vector<ipc::type>{}, OBS_service_createVideoStreamingEncoder));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_createVideoRecordingEncoder", std::vector<ipc::type>{}, OBS_service_createVideoRecordingEncoder));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_createService", std::vector<ipc::type>{}, OBS_service_createService));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_createRecordingSettings", std::vector<ipc::type>{}, OBS_service_createRecordingSettings));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_createStreamingOutput", std::vector<ipc::type>{}, OBS_service_createStreamingOutput));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_createRecordingOutput", std::vector<ipc::type>{}, OBS_service_createRecordingOutput));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_startStreaming", std::vector<ipc::type>{}, OBS_service_startStreaming));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_startRecording", std::vector<ipc::type>{}, OBS_service_startRecording));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_stopStreaming", std::vector<ipc::type>{ipc::type::Int32}, OBS_service_stopStreaming));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_stopRecording", std::vector<ipc::type>{}, OBS_service_stopRecording));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_associateAudioAndVideoToTheCurrentStreamingContext",
	    std::vector<ipc::type>{},
	    OBS_service_associateAudioAndVideoToTheCurrentStreamingContext));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_associateAudioAndVideoToTheCurrentRecordingContext",
	    std::vector<ipc::type>{},
	    OBS_service_associateAudioAndVideoToTheCurrentRecordingContext));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_associateAudioAndVideoEncodersToTheCurrentStreamingOutput",
	    std::vector<ipc::type>{},
	    OBS_service_associateAudioAndVideoEncodersToTheCurrentStreamingOutput));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_associateAudioAndVideoEncodersToTheCurrentRecordingOutput",
	    std::vector<ipc::type>{},
	    OBS_service_associateAudioAndVideoEncodersToTheCurrentRecordingOutput));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_setServiceToTheStreamingOutput",
	    std::vector<ipc::type>{},
	    OBS_service_setServiceToTheStreamingOutput));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_setRecordingSettings", std::vector<ipc::type>{}, OBS_service_setRecordingSettings));
	cls->register_function(std::make_shared<ipc::function>(
	    "OBS_service_connectOutputSignals", std::vector<ipc::type>{}, OBS_service_connectOutputSignals));
	cls->register_function(std::make_shared<ipc::function>("Query", std::vector<ipc::type>{}, Query));

	// TODO : connect output signals

	srv.register_collection(cls);
}

void OBS_service::OBS_service_resetAudioContext(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	resetAudioContext();
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_service::OBS_service_resetVideoContext(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	resetVideoContext(NULL);
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_service::OBS_service_createAudioEncoder(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	std::shared_ptr<obs_encoder_t> dummy;
	createAudioEncoder(dummy);
//	createAudioEncoder(NULL);
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_service::OBS_service_createVideoStreamingEncoder(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	createVideoStreamingEncoder();
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_service::OBS_service_createVideoRecordingEncoder(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	createVideoRecordingEncoder();
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_service::OBS_service_createService(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	createService();
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_service::OBS_service_createRecordingSettings(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	createRecordingSettings();
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_service::OBS_service_createStreamingOutput(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	createStreamingOutput();
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_service::OBS_service_createRecordingOutput(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	createRecordingOutput();
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_service::OBS_service_startStreaming(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	startStreaming();
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_service::OBS_service_startRecording(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	startRecording();
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_service::OBS_service_stopStreaming(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	stopStreaming((bool)args[0].value_union.i32);
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_service::OBS_service_stopRecording(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	stopRecording();
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_service::OBS_service_associateAudioAndVideoToTheCurrentStreamingContext(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	associateAudioAndVideoToTheCurrentStreamingContext();
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_service::OBS_service_associateAudioAndVideoToTheCurrentRecordingContext(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	associateAudioAndVideoToTheCurrentRecordingContext();
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_service::OBS_service_associateAudioAndVideoEncodersToTheCurrentStreamingOutput(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	associateAudioAndVideoEncodersToTheCurrentStreamingOutput();
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_service::OBS_service_associateAudioAndVideoEncodersToTheCurrentRecordingOutput(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	associateAudioAndVideoEncodersToTheCurrentRecordingOutput();
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_service::OBS_service_setServiceToTheStreamingOutput(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	setServiceToTheStreamingOutput();
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

void OBS_service::OBS_service_setRecordingSettings(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	setRecordingSettings();
	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
	AUTO_DEBUG;
}

bool OBS_service::resetAudioContext(void)
{
	struct obs_audio_info ai;

	config_t* basicConfig = OBS_API::openConfigFile(OBS_API::getBasicConfigPath());

	ai.samples_per_sec          = config_get_uint(basicConfig, "Audio", "SampleRate");
	const char* channelSetupStr = config_get_string(basicConfig, "Audio", "ChannelSetup");

	if (strcmp(channelSetupStr, "Mono") == 0)
		ai.speakers = SPEAKERS_MONO;
	else
		ai.speakers = SPEAKERS_STEREO;

	return obs_reset_audio(&ai);
}

static inline enum video_format GetVideoFormatFromName(const char* name)
{
	if (name != NULL) {
		if (astrcmpi(name, "I420") == 0)
			return VIDEO_FORMAT_I420;
		else if (astrcmpi(name, "NV12") == 0)
			return VIDEO_FORMAT_NV12;
		else if (astrcmpi(name, "I444") == 0)
			return VIDEO_FORMAT_I444;
#if 0 //currently unsupported
        else if (astrcmpi(name, "YVYU") == 0)
            return VIDEO_FORMAT_YVYU;
        else if (astrcmpi(name, "YUY2") == 0)
            return VIDEO_FORMAT_YUY2;
        else if (astrcmpi(name, "UYVY") == 0)
            return VIDEO_FORMAT_UYVY;
#endif
		else
			return VIDEO_FORMAT_RGBA;
	} else {
		return VIDEO_FORMAT_I420;
	}
}

static inline enum obs_scale_type GetScaleType(config_t* config)
{
	const char* scaleTypeStr = config_get_string(config, "Video", "ScaleType");

	if (scaleTypeStr != NULL) {
		if (astrcmpi(scaleTypeStr, "bilinear") == 0)
			return OBS_SCALE_BILINEAR;
		else if (astrcmpi(scaleTypeStr, "lanczos") == 0)
			return OBS_SCALE_LANCZOS;
		else
			return OBS_SCALE_BICUBIC;
	} else {
		return OBS_SCALE_BICUBIC;
	}
}

static inline const char* GetRenderModule(config_t* config)
{
	const char* renderer = config_get_string(config, "Video", "Renderer");

	const char* DL_D3D11 = "libobs-d3d11.dll";
	const char* DL_OPENGL;

#ifdef _WIN32
	DL_OPENGL = "libobs-opengl.dll";
#else
	DL_OPENGL = "libobs-opengl.so";
#endif

	if (renderer != NULL) {
		return (astrcmpi(renderer, "Direct3D 11") == 0) ? DL_D3D11 : DL_OPENGL;
	} else {
		return DL_D3D11;
	}
}

void GetFPSInteger(config_t* basicConfig, uint32_t& num, uint32_t& den)
{
	num = (uint32_t)config_get_uint(basicConfig, "Video", "FPSInt");
	den = 1;
}

void GetFPSFraction(config_t* basicConfig, uint32_t& num, uint32_t& den)
{
	num = (uint32_t)config_get_uint(basicConfig, "Video", "FPSNum");
	den = (uint32_t)config_get_uint(basicConfig, "Video", "FPSDen");
}

void GetFPSNanoseconds(config_t* basicConfig, uint32_t& num, uint32_t& den)
{
	num = 1000000000;
	den = (uint32_t)config_get_uint(basicConfig, "Video", "FPSNS");
}

void GetFPSCommon(config_t* basicConfig, uint32_t& num, uint32_t& den)
{
	const char* val = config_get_string(basicConfig, "Video", "FPSCommon");
	if (val != NULL) {
		if (strcmp(val, "10") == 0) {
			num = 10;
			den = 1;
		} else if (strcmp(val, "20") == 0) {
			num = 20;
			den = 1;
		} else if (strcmp(val, "24 NTSC") == 0) {
			num = 24000;
			den = 1001;
		} else if (strcmp(val, "25") == 0) {
			num = 25;
			den = 1;
		} else if (strcmp(val, "29.97") == 0) {
			num = 30000;
			den = 1001;
		} else if (strcmp(val, "48") == 0) {
			num = 48;
			den = 1;
		} else if (strcmp(val, "59.94") == 0) {
			num = 60000;
			den = 1001;
		} else if (strcmp(val, "60") == 0) {
			num = 60;
			den = 1;
		} else {
			num = 30;
			den = 1;
		}
	} else {
		num = 30;
		den = 1;
		config_set_uint(basicConfig, "Video", "FPSType", 0);
		config_set_string(basicConfig, "Video", "FPSCommon", "30");
		config_save_safe(basicConfig, "tmp", nullptr);
	}
}

void GetConfigFPS(config_t* basicConfig, uint32_t& num, uint32_t& den)
{
	uint32_t type = config_get_uint(basicConfig, "Video", "FPSType");
	if (type == 1) //"Integer"
		GetFPSInteger(basicConfig, num, den);
	else if (type == 2) //"Fraction"
		GetFPSFraction(basicConfig, num, den);
	else if (false) //"Nanoseconds", currently not implemented
		GetFPSNanoseconds(basicConfig, num, den);
	else
		GetFPSCommon(basicConfig, num, den);
}

/* some nice default output resolution vals */
static const double vals[] = {1.0, 1.25, (1.0 / 0.75), 1.5, (1.0 / 0.6), 1.75, 2.0, 2.25, 2.5, 2.75, 3.0};

static const size_t numVals = sizeof(vals) / sizeof(double);

bool OBS_service::resetVideoContext(const char* outputType)
{
	config_t* basicConfig = OBS_API::openConfigFile(OBS_API::getBasicConfigPath());

	obs_video_info ovi;
	std::string    gslib = "";
#ifdef _WIN32
	gslib = "libobs-d3d11.dll";
#else
	gslib     = "libobs-opengl";
#endif
	ovi.graphics_module = gslib.c_str();

	ovi.base_width  = (uint32_t)config_get_uint(basicConfig, "Video", "BaseCX");
	ovi.base_height = (uint32_t)config_get_uint(basicConfig, "Video", "BaseCY");

	const char* outputMode = config_get_string(basicConfig, "Output", "Mode");

	if (outputMode == NULL) {
		outputMode = "Simple";
	}

	ovi.output_width  = (uint32_t)config_get_uint(basicConfig, "Video", "OutputCX");
	ovi.output_height = (uint32_t)config_get_uint(basicConfig, "Video", "OutputCY");

	std::vector<Screen> resolutions = OBS_API::availableResolutions();

	if (ovi.base_width == 0 || ovi.base_height == 0) {
		for (int i = 0; i < resolutions.size(); i++) {
			if (ovi.base_width * ovi.base_height < resolutions.at(i).width * resolutions.at(i).height) {
				ovi.base_width  = resolutions.at(i).width;
				ovi.base_height = resolutions.at(i).height;
			}
		}
	}

	config_set_uint(basicConfig, "Video", "BaseCX", ovi.base_width);
	config_set_uint(basicConfig, "Video", "BaseCY", ovi.base_height);

	if (ovi.output_width == 0 || ovi.output_height == 0) {
		if (ovi.base_width > 1280 && ovi.base_height > 720) {
			int idx = 0;
			do {
				ovi.output_width  = uint32_t(double(ovi.base_width) / vals[idx]);
				ovi.output_height = uint32_t(double(ovi.base_height) / vals[idx]);
				idx++;
			} while (ovi.output_width > 1280 && ovi.output_height > 720);
		} else {
			ovi.output_width  = ovi.base_width;
			ovi.output_height = ovi.base_height;
		}

		config_set_uint(basicConfig, "Video", "OutputCX", ovi.output_width);
		config_set_uint(basicConfig, "Video", "OutputCY", ovi.output_height);
	}

	GetConfigFPS(basicConfig, ovi.fps_num, ovi.fps_den);

	const char* colorFormat = config_get_string(basicConfig, "Video", "ColorFormat");
	const char* colorSpace  = config_get_string(basicConfig, "Video", "ColorSpace");
	const char* colorRange  = config_get_string(basicConfig, "Video", "ColorRange");

	ovi.output_format = GetVideoFormatFromName(colorFormat);

	ovi.adapter        = 0;
	ovi.gpu_conversion = true;

	ovi.colorspace = astrcmpi(colorSpace, "601") == 0 ? VIDEO_CS_601 : VIDEO_CS_709;
	ovi.range      = astrcmpi(colorRange, "Full") == 0 ? VIDEO_RANGE_FULL : VIDEO_RANGE_PARTIAL;

	ovi.scale_type = GetScaleType(basicConfig);

	config_save_safe(basicConfig, "tmp", nullptr);

	return obs_reset_video(&ovi);
}

const char* FindAudioEncoderFromCodec(const char* type)
{
	const char* alt_enc_id = nullptr;
	size_t      i          = 0;

	while (obs_enum_encoder_types(i++, &alt_enc_id)) {
		const char* codec = obs_get_encoder_codec(alt_enc_id);
		if (strcmp(type, codec) == 0) {
			return alt_enc_id;
		}
	}

	return nullptr;
}

void OBS_service::createAudioEncoder(std::shared_ptr<obs_encoder_t>& audioEncoder)
{
	config_t* basicConfig = OBS_API::openConfigFile(OBS_API::getBasicConfigPath());

	int bitrate = FindClosestAvailableAACBitrate((int)config_get_uint(basicConfig, "SimpleOutput", "ABitrate"));

	const char* id = GetAACEncoderForBitrate(bitrate);
	if (!id) {
		audioEncoder = nullptr;
		return;
	}

	// if (s_usingRecordingPreset)
		// obs_encoder_release(audioEncoder);

	audioEncoder = std::shared_ptr<obs_encoder_t>(
	    obs_audio_encoder_create(id, "simple_audio", nullptr, 0, nullptr), default_delete<obs_encoder_t>());
}

void OBS_service::createVideoStreamingEncoder()
{
	std::string basicConfigFile = OBS_API::getBasicConfigPath();
	config_t*   config          = OBS_API::openConfigFile(basicConfigFile);

	const char* encoder = config_get_string(config, "SimpleOutput", "StreamEncoder");

	if (encoder == NULL) {
		encoder = "obs_x264";
	}

	s_videoStreamingEncoder = std::shared_ptr<obs_encoder_t>(
	    obs_video_encoder_create(encoder, "streaming_h264", nullptr, nullptr), default_delete<obs_encoder_t>());

	if (config == nullptr) {
		obs_data_t* h264Settings = obs_data_create();
		config                   = config_create(basicConfigFile.c_str());
		h264Settings             = obs_encoder_defaults("obs_x264");
		config_set_uint(config, "SimpleOutput", "VBitrate", 2500);
		config_save_safe(config, "tmp", nullptr);
		obs_data_release(h264Settings);
	} else {
		updateVideoStreamingEncoder();
	}
}

static inline bool valid_string(const char* str)
{
	while (str && *str) {
		if (*(str++) != ' ')
			return true;
	}

	return false;
}
static void replace_text(struct dstr* str, size_t pos, size_t len, const char* new_text)
{
	struct dstr front = {0};
	struct dstr back  = {0};

	dstr_left(&front, str, pos);
	dstr_right(&back, str, pos + len);
	dstr_copy_dstr(str, &front);
	dstr_cat(str, new_text);
	dstr_cat_dstr(str, &back);
	dstr_free(&front);
	dstr_free(&back);
}

static void erase_ch(struct dstr* str, size_t pos)
{
	struct dstr new_str = {0};
	dstr_left(&new_str, str, pos);
	dstr_cat(&new_str, str->array + pos + 1);
	dstr_free(str);
	*str = new_str;
}

char* os_generate_formatted_filename(const char* extension, bool space, const char* format)
{
	time_t     now = time(0);
	struct tm* cur_time;
	cur_time = localtime(&now);

	const size_t       spec_count = 23;
	static const char* spec[][2]  = {
        {"%CCYY", "%Y"}, {"%YY", "%y"}, {"%MM", "%m"}, {"%DD", "%d"}, {"%hh", "%H"},
        {"%mm", "%M"},   {"%ss", "%S"}, {"%%", "%%"},

        {"%a", ""},      {"%A", ""},    {"%b", ""},    {"%B", ""},    {"%d", ""},
        {"%H", ""},      {"%I", ""},    {"%m", ""},    {"%M", ""},    {"%p", ""},
        {"%S", ""},      {"%y", ""},    {"%Y", ""},    {"%z", ""},    {"%Z", ""},
    };

	char        convert[128] = {0};
	struct dstr sf;
	struct dstr c   = {0};
	size_t      pos = 0;

	dstr_init_copy(&sf, format);

	while (pos < sf.len) {
		for (size_t i = 0; i < spec_count && !convert[0]; i++) {
			size_t len = strlen(spec[i][0]);

			const char* cmp = sf.array + pos;

			if (astrcmp_n(cmp, spec[i][0], len) == 0) {
				if (strlen(spec[i][1]))
					strftime(convert, sizeof(convert), spec[i][1], cur_time);
				else
					strftime(convert, sizeof(convert), spec[i][0], cur_time);

				dstr_copy(&c, convert);
				if (c.len && valid_string(c.array))
					replace_text(&sf, pos, len, convert);
			}
		}

		if (convert[0]) {
			pos += strlen(convert);
			convert[0] = 0;
		} else if (!convert[0] && sf.array[pos] == '%') {
			erase_ch(&sf, pos);
		} else {
			pos++;
		}
	}

	if (!space)
		dstr_replace(&sf, " ", "_");

	dstr_cat_ch(&sf, '.');
	dstr_cat(&sf, extension);
	dstr_free(&c);

	if (sf.len > 255)
		dstr_mid(&sf, &sf, 0, 255);

	return sf.array;
}

std::string GenerateSpecifiedFilename(const char* extension, bool noSpace, const char* format)
{
	char* filename = os_generate_formatted_filename(extension, !noSpace, format);

	std::string result(filename);

	bfree(filename);

	return result;
}

static void ensure_directory_exists(string& path)
{
	replace(path.begin(), path.end(), '\\', '/');

	size_t last = path.rfind('/');
	if (last == string::npos)
		return;

	string directory = path.substr(0, last);
	os_mkdirs(directory.c_str());
}

static void FindBestFilename(string& strPath, bool noSpace)
{
	int num = 2;

	if (!os_file_exists(strPath.c_str()))
		return;

	const char* ext = strrchr(strPath.c_str(), '.');
	if (!ext)
		return;

	int extStart = int(ext - strPath.c_str());
	for (;;) {
		string testPath = strPath;
		string numStr;

		numStr = noSpace ? "_" : " (";
		numStr += to_string(num++);
		if (!noSpace)
			numStr += ")";

		testPath.insert(extStart, numStr);

		if (!os_file_exists(testPath.c_str())) {
			strPath = testPath;
			break;
		}
	}
}

static void remove_reserved_file_characters(string& s)
{
	replace(s.begin(), s.end(), '/', '_');
	replace(s.begin(), s.end(), '\\', '_');
	replace(s.begin(), s.end(), '*', '_');
	replace(s.begin(), s.end(), '?', '_');
	replace(s.begin(), s.end(), '"', '_');
	replace(s.begin(), s.end(), '|', '_');
	replace(s.begin(), s.end(), ':', '_');
	replace(s.begin(), s.end(), '>', '_');
	replace(s.begin(), s.end(), '<', '_');
}

void OBS_service::createVideoRecordingEncoder()
{
	s_videoRecordingEncoder = std::shared_ptr<obs_encoder_t>(
	    obs_video_encoder_create("obs_x264", "simple_h264_recording", nullptr, nullptr), default_delete<obs_encoder_t>());
}

void OBS_service::createService()
{
	const char* type;

	struct stat buffer;
	std::string serviceConfigFile = OBS_API::getServiceConfigPath();
	bool        fileExist         = os_stat(serviceConfigFile.c_str(), &buffer) == 0;

	obs_data_t* data;
	obs_data_t* settings;
	obs_data_t* hotkey_data;

	if (!fileExist) {
		s_service = std::shared_ptr<obs_service_t>(
		    obs_service_create("rtmp_common", "default_service", nullptr, nullptr), default_delete<obs_service_t>());
		data     = obs_data_create();
		settings = obs_service_get_settings(s_service.get());

		obs_data_set_string(settings, "streamType", "rtmp_common");
		obs_data_set_string(settings, "s_service", "Twitch");
		obs_data_set_bool(settings, "show_all", 0);
		obs_data_set_string(settings, "server", "auto");
		obs_data_set_string(settings, "key", "");

		obs_data_set_string(data, "type", obs_service_get_type(s_service.get()));
		obs_data_set_obj(data, "settings", settings);

	} else {
		data = obs_data_create_from_json_file_safe(serviceConfigFile.c_str(), "bak");

		obs_data_set_default_string(data, "type", "rtmp_common");
		type = obs_data_get_string(data, "type");

		settings    = obs_data_get_obj(data, "settings");
		hotkey_data = obs_data_get_obj(data, "hotkeys");

		s_service = std::shared_ptr<obs_service_t>(
		    obs_service_create(type, "default_service", settings, hotkey_data), default_delete<obs_service_t>());

		obs_data_release(hotkey_data);
	}

	if (!obs_data_save_json_safe(data, serviceConfigFile.c_str(), "tmp", "bak")) {
		blog(LOG_WARNING, "Failed to save s_service %s", serviceConfigFile.c_str());
	}

	obs_data_release(settings);
	obs_data_release(data);
}

obs_data_t* OBS_service::createRecordingSettings(void)
{
	obs_data_t* settings = obs_data_create();
	/*obs_data_set_string(settings, "format_name", "avi");
    obs_data_set_string(settings, "video_encoder", "utvideo");
    obs_data_set_string(settings, "audio_encoder", "pcm_s16le");
    obs_data_set_string(settings, "path", "./recording_1.avi");*/
	// obs_data_t *settings = obs_encoder_get_settings(s_videoRecordingEncoder);

	return settings;
}

void OBS_service::createStreamingOutput(void)
{
	s_streamingOutput =
	    std::shared_ptr<obs_output_t>(
	    obs_output_create("rtmp_output", "simple_stream", nullptr, nullptr), default_delete<obs_output_t>());
	connectOutputSignals();
}

void OBS_service::createRecordingOutput(void)
{
	s_recordingOutput = std::shared_ptr<obs_output_t>(
	    obs_output_create("ffmpeg_muxer", "simple_file_output", nullptr, nullptr), default_delete<obs_output_t>());
	connectOutputSignals();
}

bool OBS_service::startStreaming(void)
{
	const char* type = obs_service_get_output_type(s_service.get());
	if (!type)
		type = "rtmp_output";

	s_streamingOutput =
	    std::shared_ptr<obs_output_t>(
	    obs_output_create(type, "simple_stream", nullptr, nullptr), default_delete<obs_output_t>());
	connectOutputSignals();

	std::string basicConfigFile = OBS_API::getBasicConfigPath();
	config_t*   config          = OBS_API::openConfigFile(basicConfigFile);

	int trackIndex = config_get_int(config, "AdvOut", "TrackIndex");

	const char* codec = obs_output_get_supported_audio_codecs(s_streamingOutput.get());
	if (!codec) {
		return false;
	}

	if (strcmp(codec, "aac") == 0) {
		createAudioEncoder(s_audioStreamingEncoder);
	} else {
		const char* id           = FindAudioEncoderFromCodec(codec);
		int         audioBitrate = GetAudioBitrate();
		obs_data_t* settings     = obs_data_create();
		obs_data_set_int(settings, "bitrate", audioBitrate);

		s_audioStreamingEncoder = std::shared_ptr<obs_encoder_t>(
		    obs_audio_encoder_create(id, "alt_audio_enc", nullptr, trackIndex - 1, nullptr),
		    default_delete<obs_encoder_t>());
		if (!s_audioStreamingEncoder) {
			obs_data_release(settings);
			return false;
		}

		obs_encoder_update(s_audioStreamingEncoder.get(), settings);
		obs_encoder_set_audio(s_audioStreamingEncoder.get(), obs_get_audio());

		obs_data_release(settings);
	}

	updateService();
	updateStreamSettings();

	s_isStreaming = true;
	return obs_output_start(s_streamingOutput.get());
}

bool OBS_service::startRecording(void)
{
	s_isRecording = true;
	createAudioEncoder(s_audioRecordingEncoder);
	updateRecordSettings();

	if (!obs_output_start(s_recordingOutput.get())) {
		SignalInfo signal = SignalInfo("recording", "stop");
		s_isRecording       = false;
	}
	return s_isRecording;
}

void OBS_service::stopStreaming(bool forceStop)
{
	if (forceStop)
		obs_output_force_stop(s_streamingOutput.get());
	else
		obs_output_stop(s_streamingOutput.get());
	s_isStreaming = false;
}

void OBS_service::stopRecording(void)
{
	obs_output_stop(s_recordingOutput.get());
	s_isRecording = false;
}

void OBS_service::associateAudioAndVideoToTheCurrentStreamingContext(void)
{
	std::string basicConfigFile = OBS_API::getBasicConfigPath();
	config_t*   config          = OBS_API::openConfigFile(basicConfigFile);

	const char* advancedMode = config_get_string(config, "Output", "Mode");

	if (strcmp(advancedMode, "Advanced") == 0) {
		unsigned int cx = 0;
		unsigned int cy = 0;

		bool        rescale    = config_get_bool(config, "AdvOut", "Rescale");
		const char* rescaleRes = config_get_string(config, "AdvOut", "RescaleRes");

		if (rescale && rescaleRes && *rescaleRes) {
			if (sscanf(rescaleRes, "%ux%u", &cx, &cy) != 2) {
				cx = 0;
				cy = 0;
			}
			obs_encoder_set_scaled_size(s_videoStreamingEncoder.get(), cx, cy);
		}
	}

	obs_encoder_set_video(s_videoStreamingEncoder.get(), obs_get_video());
	obs_encoder_set_audio(s_audioStreamingEncoder.get(), obs_get_audio());
}

void OBS_service::associateAudioAndVideoToTheCurrentRecordingContext(void)
{
	std::string basicConfigFile = OBS_API::getBasicConfigPath();
	config_t*   config          = OBS_API::openConfigFile(basicConfigFile);

	const char* advancedMode = config_get_string(config, "Output", "Mode");

	if (strcmp(advancedMode, "Advanced") == 0) {
		unsigned int cx = 0;
		unsigned int cy = 0;

		bool        rescale    = config_get_bool(config, "AdvOut", "RecRescale");
		const char* rescaleRes = config_get_string(config, "AdvOut", "RecRescaleRes");

		if (rescale && rescaleRes && *rescaleRes) {
			if (sscanf(rescaleRes, "%ux%u", &cx, &cy) != 2) {
				cx = 0;
				cy = 0;
			}
			obs_encoder_set_scaled_size(s_videoRecordingEncoder.get(), cx, cy);
		}
	}

	obs_encoder_set_video(s_videoRecordingEncoder.get(), obs_get_video());
	obs_encoder_set_audio(s_audioRecordingEncoder.get(), obs_get_audio());
}

void OBS_service::associateAudioAndVideoEncodersToTheCurrentStreamingOutput(void)
{
	obs_output_set_video_encoder(s_streamingOutput.get(), s_videoStreamingEncoder.get());
	obs_output_set_audio_encoder(s_streamingOutput.get(), s_audioStreamingEncoder.get(), 0);
}

void OBS_service::associateAudioAndVideoEncodersToTheCurrentRecordingOutput(void)
{
	obs_output_set_video_encoder(s_recordingOutput.get(), s_videoRecordingEncoder.get());
	obs_output_set_audio_encoder(s_recordingOutput.get(), s_audioRecordingEncoder.get(), 0);
}

void OBS_service::setServiceToTheStreamingOutput(void)
{
	obs_output_set_service(s_streamingOutput.get(), s_service.get());
}

void OBS_service::setRecordingSettings(void)
{
	/* obs_data_t *settings = createRecordingSettings();
    obs_output_update(s_recordingOutput, settings);
    obs_data_release(settings); */
}

std::shared_ptr<obs_service_t> OBS_service::getService(void)
{
	if (!obs_initialized())
		throw "Trying to get an obs object but it isn't active";
	const char* serviceType = obs_service_get_type(s_service.get());
	return s_service;
}

void OBS_service::setService(obs_service_t* newService)
{
	s_service = std::shared_ptr<obs_service_t>(newService, default_delete<obs_service_t>());
}

void OBS_service::saveService(void)
{
	if (!s_service)
		return;

	obs_data_t* data     = obs_data_create();
	obs_data_t* settings = obs_service_get_settings(s_service.get());

	const char* serviceType = obs_service_get_type(s_service.get());

	obs_data_set_string(data, "type", obs_service_get_type(s_service.get()));
	obs_data_set_obj(data, "settings", settings);

	if (!obs_data_save_json_safe(data, OBS_API::getServiceConfigPath().c_str(), "tmp", "bak"))
		blog(LOG_WARNING, "Failed to save s_service");

	obs_service_update(s_service.get(), settings);

	serviceType = obs_service_get_type(s_service.get());

	// obs_data_release(settings);
	obs_data_release(data);
}

bool OBS_service::isStreamingOutputActive(void)
{
	return obs_output_active(s_streamingOutput.get());
}

int OBS_service::GetAudioBitrate()
{
	std::string basicConfigFile = OBS_API::getBasicConfigPath();
	config_t*   config          = OBS_API::openConfigFile(basicConfigFile);

	int bitrate = (int)config_get_uint(config, "SimpleOutput", "ABitrate");

	return FindClosestAvailableAACBitrate(bitrate);
}

static bool EncoderAvailable(const char* encoder)
{
	const char* val;
	int         i = 0;

	while (obs_enum_encoder_types(i++, &val)) {
		if (strcmp(val, encoder) == 0)
			return true;
	}

	return false;
}

void OBS_service::updateVideoStreamingEncoder()
{
	std::string basicConfigFile = OBS_API::getBasicConfigPath();
	config_t*   config          = OBS_API::openConfigFile(basicConfigFile);

	obs_data_t* h264Settings = obs_data_create();
	obs_data_t* aacSettings  = obs_data_create();

	int         videoBitrate   = config_get_uint(config, "SimpleOutput", "VBitrate");
	int         audioBitrate   = GetAudioBitrate();
	bool        advanced       = config_get_bool(config, "SimpleOutput", "UseAdvanced");
	bool        enforceBitrate = config_get_bool(config, "SimpleOutput", "EnforceBitrate");
	const char* custom         = config_get_string(config, "SimpleOutput", "x264Settings");
	const char* encoder        = config_get_string(config, "SimpleOutput", "StreamEncoder");
	const char* encoderID;
	const char* presetType;
	const char* preset;

	if (encoder != NULL) {
		if (strcmp(encoder, SIMPLE_ENCODER_QSV) == 0 || strcmp(encoder, ADVANCED_ENCODER_QSV) == 0) {
			presetType = "QSVPreset";
			encoderID = "obs_qsv11";
		} else if (strcmp(encoder, SIMPLE_ENCODER_AMD) == 0 || strcmp(encoder, ADVANCED_ENCODER_AMD) == 0) {
			presetType = "AMDPreset";
			UpdateStreamingSettings_amd(h264Settings, videoBitrate);
			encoderID = "amd_amf_h264";
		} else if (strcmp(encoder, SIMPLE_ENCODER_NVENC) == 0 || strcmp(encoder, ADVANCED_ENCODER_NVENC) == 0) {
			presetType = "NVENCPreset";
			encoderID  = "ffmpeg_nvenc";
		} else {
			presetType = "Preset";
			encoderID  = "obs_x264";
		}
		preset = config_get_string(config, "SimpleOutput", presetType);

		// if (s_videoStreamingEncoder != NULL && s_usingRecordingPreset) 
			// s_videoStreamingEncoder.release();
		
		s_videoStreamingEncoder = std::shared_ptr<obs_encoder_t>(
		    obs_video_encoder_create(encoderID, "streaming_h264", nullptr, nullptr), default_delete<obs_encoder_t>());
	}

	if (videoBitrate == 0) {
		videoBitrate = 2500;
		config_set_uint(config, "SimpleOutput", "VBitrate", videoBitrate);
		config_save_safe(config, "tmp", nullptr);
	}

	obs_data_set_string(h264Settings, "rate_control", "CBR");
	obs_data_set_int(h264Settings, "bitrate", videoBitrate);

	if (advanced) {
		obs_data_set_string(h264Settings, "preset", preset);
		obs_data_set_string(h264Settings, "x264opts", custom);
	}

	obs_data_set_string(aacSettings, "rate_control", "CBR");
	obs_data_set_int(aacSettings, "bitrate", audioBitrate);

	const char* url = obs_service_get_url(s_service.get());

	obs_service_apply_encoder_settings(s_service.get(), h264Settings, aacSettings);

	if (advanced && !enforceBitrate) {
		obs_data_set_int(h264Settings, "bitrate", videoBitrate);
		obs_data_set_int(aacSettings, "bitrate", audioBitrate);
	}

	video_t*          video  = obs_get_video();
	enum video_format format = video_output_get_format(video);

	if (format != VIDEO_FORMAT_NV12 && format != VIDEO_FORMAT_I420)
		obs_encoder_set_preferred_video_format(s_videoStreamingEncoder.get(), VIDEO_FORMAT_NV12);

	obs_encoder_update(s_videoStreamingEncoder.get(), h264Settings);
	obs_encoder_update(s_audioStreamingEncoder.get(), aacSettings);

	obs_data_release(h264Settings);
	obs_data_release(aacSettings);
}

std::string OBS_service::GetDefaultVideoSavePath(void)
{
	wchar_t path_utf16[MAX_PATH];
	char    path_utf8[MAX_PATH] = {};

	SHGetFolderPathW(NULL, CSIDL_MYVIDEO, NULL, SHGFP_TYPE_CURRENT, path_utf16);

	os_wcs_to_utf8(path_utf16, wcslen(path_utf16), path_utf8, MAX_PATH);
	return std::string(path_utf8);
}

void OBS_service::updateService(void)
{
	setServiceToTheStreamingOutput();
}

void OBS_service::updateStreamingOutput(void)
{
	updateVideoStreamingEncoder();

	associateAudioAndVideoToTheCurrentStreamingContext();
	associateAudioAndVideoEncodersToTheCurrentStreamingOutput();
}

void OBS_service::updateRecordingOutput(void)
{
	std::string basicConfigFile = OBS_API::getBasicConfigPath();
	config_t*   config          = OBS_API::openConfigFile(basicConfigFile);

	const char* path              = config_get_string(config, "SimpleOutput", "FilePath");
	const char* format            = config_get_string(config, "SimpleOutput", "RecFormat");
	const char* mux               = config_get_string(config, "SimpleOutput", "MuxerCustom");
	bool        noSpace           = config_get_bool(config, "SimpleOutput", "FileNameWithoutSpace");
	const char* filenameFormat    = config_get_string(config, "Output", "FilenameFormatting");
	bool        overwriteIfExists = config_get_bool(config, "Output", "OverwriteIfExists");
	const char* rbPrefix          = config_get_string(config, "SimpleOutput", "RecRBPrefix");
	const char* rbSuffix          = config_get_string(config, "SimpleOutput", "RecRBSuffix");
	int         rbTime            = config_get_int(config, "SimpleOutput", "RecRBTime");
	int         rbSize            = config_get_int(config, "SimpleOutput", "RecRBSize");

	os_dir_t* dir = path && path[0] ? os_opendir(path) : nullptr;

	if (filenameFormat == NULL) {
		filenameFormat = "%CCYY-%MM-%DD %hh-%mm-%ss";
	}
	string strPath;
	strPath += path;

	char lastChar = strPath.back();
	if (lastChar != '/' && lastChar != '\\')
		strPath += "/";

	bool s_ffmpegOutput         = false;
	bool s_usingRecordingPreset = true;

	if (filenameFormat != NULL && format != NULL) {
		strPath += GenerateSpecifiedFilename(s_ffmpegOutput ? "avi" : format, noSpace, filenameFormat);
		if (!strPath.empty())
			ensure_directory_exists(strPath);
	}
	if (!overwriteIfExists)
		FindBestFilename(strPath, noSpace);

	obs_data_t* settings        = obs_data_create();
	bool        useReplayBuffer = config_get_bool(config, "SimpleOutput", "RecRB");

	if (useReplayBuffer) {
		string f;

		if (rbPrefix && *rbPrefix) {
			f += rbPrefix;
			if (f.back() != ' ')
				f += " ";
		}

		f += filenameFormat;

		if (rbSuffix && *rbSuffix) {
			if (*rbSuffix != ' ')
				f += " ";
			f += rbSuffix;
		}

		remove_reserved_file_characters(f);

		obs_data_set_string(settings, "directory", path);
		obs_data_set_string(settings, "extension", format);
		obs_data_set_int(settings, "max_time_sec", rbTime);
		obs_data_set_int(settings, "max_size_mb", s_usingRecordingPreset ? rbSize : 0);
	} else {
		obs_data_set_string(settings, s_ffmpegOutput ? "url" : "path", strPath.c_str());
	}

	obs_output_update(s_recordingOutput.get(), settings);
	obs_data_release(settings);
}

void OBS_service::updateAdvancedRecordingOutput(void)
{
	std::string basicConfigFile = OBS_API::getBasicConfigPath();
	config_t*   config          = OBS_API::openConfigFile(basicConfigFile);

	const char* path       = config_get_string(config, "AdvOut", "RecFilePath");
	const char* mux        = config_get_string(config, "AdvOut", "RecMuxerCustom");
	bool        rescale    = config_get_bool(config, "AdvOut", "RecRescale");
	const char* rescaleRes = config_get_string(config, "AdvOut", "RecRescaleRes");
	int         tracks     = config_get_int(config, "AdvOut", "RecTracks");

	const char* recFormat;
	const char* filenameFormat;
	bool        noSpace           = false;
	bool        overwriteIfExists = false;

	recFormat         = config_get_string(config, "AdvOut", "RecFormat");
	filenameFormat    = config_get_string(config, "Output", "FilenameFormatting");
	overwriteIfExists = config_get_bool(config, "Output", "OverwriteIfExists");
	noSpace           = config_get_bool(config, "AdvOut", "RecFileNameWithoutSpace");

	os_dir_t* dir = path && path[0] ? os_opendir(path) : nullptr;

	os_closedir(dir);

	string strPath;
	strPath += path;

	char lastChar = strPath.back();
	if (lastChar != '/' && lastChar != '\\')
		strPath += "/";

	strPath += GenerateSpecifiedFilename(recFormat, noSpace, filenameFormat);
	ensure_directory_exists(strPath);
	if (!overwriteIfExists)
		FindBestFilename(strPath, noSpace);

	obs_data_t*  settings = obs_data_create();
	unsigned int cx       = 0;
	unsigned int cy       = 0;
	int          idx      = 0;

	// To be changed to the actual value
	bool useStreamEncoder = false;

	if (useStreamEncoder) {
		obs_output_set_video_encoder(s_recordingOutput.get(), s_videoStreamingEncoder.get());
	} else {
		if (rescale && rescaleRes && *rescaleRes) {
			if (sscanf(rescaleRes, "%ux%u", &cx, &cy) != 2) {
				cx = 0;
				cy = 0;
			}
		}

		obs_encoder_set_scaled_size(s_videoRecordingEncoder.get(), cx, cy);
		obs_encoder_set_video(s_videoRecordingEncoder.get(), obs_get_video());
		obs_output_set_video_encoder(s_recordingOutput.get(), s_videoRecordingEncoder.get());
	}

	// for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
	// 	if ((tracks & (1<<i)) != 0) {
	// 		obs_output_set_audio_encoder(s_recordingOutput, aacTrack[i],
	// 				idx++);
	// 	}
	// }

	obs_data_set_string(settings, "path", strPath.c_str());
	obs_data_set_string(settings, "muxer_settings", mux);
	obs_output_update(s_recordingOutput.get(), settings);
	obs_data_release(settings);
}

void OBS_service::LoadRecordingPreset_Lossless()
{
	s_recordingOutput = std::shared_ptr<obs_output_t>(
	    obs_output_create("ffmpeg_output", "simple_ffmpeg_output", nullptr, nullptr), default_delete<obs_output_t>());
	connectOutputSignals();
	if (!s_recordingOutput)
		throw "Failed to create recording FFmpeg output "
		      "(simple output)";
	// obs_output_release(s_recordingOutput);

	obs_data_t* settings = obs_data_create();
	obs_data_set_string(settings, "format_name", "avi");
	obs_data_set_string(settings, "video_encoder", "utvideo");
	obs_data_set_string(settings, "audio_encoder", "pcm_s16le");

	obs_output_update(s_recordingOutput.get(), settings);
	obs_data_release(settings);
}

void OBS_service::LoadRecordingPreset_h264(const char* encoderId)
{
	s_videoRecordingEncoder = std::shared_ptr<obs_encoder_t>(
	    obs_video_encoder_create(encoderId, "simple_h264_recording", nullptr, nullptr), 
		default_delete<obs_encoder_t>());
	if (!s_videoRecordingEncoder)
		throw "Failed to create h264 recording encoder (simple output)";
	// obs_encoder_release(s_videoRecordingEncoder);
}

static bool update_ffmpeg_output(config_t* config)
{
	if (config_has_user_value(config, "AdvOut", "FFOutputToFile"))
		return false;

	const char* url = config_get_string(config, "AdvOut", "FFURL");
	if (!url)
		return false;

	bool isActualURL = strstr(url, "://") != nullptr;
	if (isActualURL)
		return false;

	string urlStr = url;
	string extension;

	for (size_t i = urlStr.length(); i > 0; i--) {
		size_t idx = i - 1;

		if (urlStr[idx] == '.') {
			extension = &urlStr[i];
		}

		if (urlStr[idx] == '\\' || urlStr[idx] == '/') {
			urlStr[idx] = 0;
			break;
		}
	}

	if (urlStr.empty() || extension.empty())
		return false;

	config_remove_value(config, "AdvOut", "FFURL");
	config_set_string(config, "AdvOut", "FFFilePath", urlStr.c_str());
	config_set_string(config, "AdvOut", "FFExtension", extension.c_str());
	config_set_bool(config, "AdvOut", "FFOutputToFile", true);
	return true;
}

void OBS_service::UpdateFFmpegOutput(void)
{
	std::string basicConfigFile = OBS_API::getBasicConfigPath();
	config_t*   config          = OBS_API::openConfigFile(basicConfigFile);

	update_ffmpeg_output(config);

	s_recordingOutput = std::shared_ptr<obs_output_t>(
	    obs_output_create("ffmpeg_output", "simple_ffmpeg_output", nullptr, nullptr), default_delete<obs_output_t>());
	connectOutputSignals();

	const char* url        = config_get_string(config, "AdvOut", "FFURL");
	int         vBitrate   = config_get_int(config, "AdvOut", "FFVBitrate");
	int         gopSize    = config_get_int(config, "AdvOut", "FFVGOPSize");
	bool        rescale    = config_get_bool(config, "AdvOut", "FFRescale");
	const char* rescaleRes = config_get_string(config, "AdvOut", "FFRescaleRes");
	const char* formatName = config_get_string(config, "AdvOut", "FFFormat");
	const char* mimeType   = config_get_string(config, "AdvOut", "FFFormatMimeType");
	const char* muxCustom  = config_get_string(config, "AdvOut", "FFMCustom");
	const char* vEncoder   = config_get_string(config, "AdvOut", "FFVEncoder");
	int         vEncoderId = config_get_int(config, "AdvOut", "FFVEncoderId");
	const char* vEncCustom = config_get_string(config, "AdvOut", "FFVCustom");
	int         aBitrate   = config_get_int(config, "AdvOut", "FFABitrate");
	int         aTrack     = config_get_int(config, "AdvOut", "FFAudioTrack");
	const char* aEncoder   = config_get_string(config, "AdvOut", "FFAEncoder");
	int         aEncoderId = config_get_int(config, "AdvOut", "FFAEncoderId");
	const char* aEncCustom = config_get_string(config, "AdvOut", "FFACustom");
	obs_data_t* settings   = obs_data_create();

	obs_data_set_string(settings, "url", url);
	obs_data_set_string(settings, "format_name", formatName);
	obs_data_set_string(settings, "format_mime_type", mimeType);
	obs_data_set_string(settings, "muxer_settings", muxCustom);
	obs_data_set_int(settings, "gop_size", gopSize);
	obs_data_set_int(settings, "video_bitrate", vBitrate);
	obs_data_set_string(settings, "video_encoder", vEncoder);
	obs_data_set_int(settings, "video_encoder_id", vEncoderId);
	obs_data_set_string(settings, "video_settings", vEncCustom);
	obs_data_set_int(settings, "audio_bitrate", aBitrate);
	obs_data_set_string(settings, "audio_encoder", aEncoder);
	obs_data_set_int(settings, "audio_encoder_id", aEncoderId);
	obs_data_set_string(settings, "audio_settings", aEncCustom);

	if (rescale && rescaleRes && *rescaleRes) {
		int width;
		int height;
		int val = sscanf(rescaleRes, "%dx%d", &width, &height);

		if (val == 2 && width && height) {
			obs_data_set_int(settings, "scale_width", width);
			obs_data_set_int(settings, "scale_height", height);
		}
	}

	obs_output_set_mixer(s_recordingOutput.get(), aTrack - 1);
	obs_output_set_media(s_recordingOutput.get(), obs_get_video(), obs_get_audio());
	obs_output_update(s_recordingOutput.get(), settings);

	obs_data_release(settings);
}

void OBS_service::updateVideoRecordingEncoder()
{
	std::string basicConfigFile = OBS_API::getBasicConfigPath();
	config_t*   config          = OBS_API::openConfigFile(basicConfigFile);

	const char* quality = config_get_string(config, "SimpleOutput", "RecQuality");
	const char* encoder = config_get_string(config, "SimpleOutput", "RecEncoder");

	s_videoEncoder = encoder;
	s_videoQuality = quality;
	s_ffmpegOutput = false;

	if (strcmp(quality, "Stream") == 0) {
		if (!s_isStreaming) {
			updateVideoStreamingEncoder();
		}
		if (s_videoRecordingEncoder != s_videoStreamingEncoder) {
			s_videoRecordingEncoder = s_videoStreamingEncoder;
			s_usingRecordingPreset  = false;
		}
		return;

	} else if (strcmp(quality, "Lossless") == 0) {
		LoadRecordingPreset_Lossless();
		s_usingRecordingPreset = true;
		s_ffmpegOutput         = true;
		UpdateRecordingSettings();
		return;

	} else {
		s_lowCPUx264 = false;
		if (strcmp(encoder, SIMPLE_ENCODER_X264) == 0 || strcmp(encoder, ADVANCED_ENCODER_X264) == 0) {
			LoadRecordingPreset_h264("obs_x264");
		} else if (strcmp(encoder, SIMPLE_ENCODER_X264_LOWCPU) == 0) {
			LoadRecordingPreset_h264("obs_x264");
			s_lowCPUx264 = true;
		} else if (strcmp(encoder, SIMPLE_ENCODER_QSV) == 0 || strcmp(encoder, ADVANCED_ENCODER_QSV) == 0) {
			LoadRecordingPreset_h264("obs_qsv11");
		} else if (strcmp(encoder, SIMPLE_ENCODER_AMD) == 0 || strcmp(encoder, ADVANCED_ENCODER_AMD) == 0) {
			LoadRecordingPreset_h264("amd_amf_h264");
		} else if (strcmp(encoder, SIMPLE_ENCODER_NVENC) == 0 || strcmp(encoder, ADVANCED_ENCODER_NVENC) == 0) {
			LoadRecordingPreset_h264("ffmpeg_nvenc");
		}
		s_usingRecordingPreset = true;

		// if (!CreateAACEncoder(aacRecording, s_aacRecEncID, 192,
		// 			"simple_aac_recording", 0))
		// 	throw "Failed to create aac recording encoder "
		// 	      "(simple output)";
	}
	UpdateRecordingSettings();
}

static bool icq_available(obs_encoder_t* encoder)
{
	obs_properties_t* props     = obs_encoder_properties(encoder);
	obs_property_t*   p         = obs_properties_get(props, "rate_control");
	bool              icq_found = false;

	size_t num = obs_property_list_item_count(p);
	for (size_t i = 0; i < num; i++) {
		const char* val = obs_property_list_item_string(p, i);
		if (strcmp(val, "ICQ") == 0) {
			icq_found = true;
			break;
		}
	}

	obs_properties_destroy(props);
	return icq_found;
}

void OBS_service::UpdateRecordingSettings_qsv11(int crf)
{
	bool icq = icq_available(s_videoRecordingEncoder.get());

	obs_data_t* settings = obs_data_create();
	obs_data_set_string(settings, "profile", "high");

	if (icq) {
		obs_data_set_string(settings, "rate_control", "ICQ");
		obs_data_set_int(settings, "icq_quality", crf);
	} else {
		obs_data_set_string(settings, "rate_control", "CQP");
		obs_data_set_int(settings, "qpi", crf);
		obs_data_set_int(settings, "qpp", crf);
		obs_data_set_int(settings, "qpb", crf);
	}

	obs_encoder_update(s_videoRecordingEncoder.get(), settings);

	obs_data_release(settings);
}

void OBS_service::UpdateRecordingSettings_nvenc(int cqp)
{
	obs_data_t* settings = obs_data_create();
	obs_data_set_string(settings, "rate_control", "CQP");
	obs_data_set_string(settings, "profile", "high");
	obs_data_set_string(settings, "preset", "hq");
	obs_data_set_int(settings, "cqp", cqp);

	obs_encoder_update(s_videoRecordingEncoder.get(), settings);

	obs_data_release(settings);
}

void OBS_service::UpdateStreamingSettings_amd(obs_data_t* settings, int bitrate)
{
	// Static Properties
	obs_data_set_int(settings, "Usage", 0);
	obs_data_set_int(settings, "Profile", 100); // High

	// Rate Control Properties
	obs_data_set_int(settings, "RateControlMethod", 3);
	obs_data_set_int(settings, "Bitrate.Target", bitrate);
	obs_data_set_int(settings, "FillerData", 1);
	obs_data_set_int(settings, "VBVBuffer", 1);
	obs_data_set_int(settings, "VBVBuffer.Size", bitrate);

	// Picture Control Properties
	obs_data_set_double(settings, "KeyframeInterval", 2.0);
	obs_data_set_int(settings, "BFrame.Pattern", 0);
}

void OBS_service::UpdateRecordingSettings_amd_cqp(int cqp)
{
	obs_data_t* settings = obs_data_create();

	// Static Properties
	obs_data_set_int(settings, "Usage", 0);
	obs_data_set_int(settings, "Profile", 100); // High

	// Rate Control Properties
	obs_data_set_int(settings, "RateControlMethod", 0);
	obs_data_set_int(settings, "QP.IFrame", cqp);
	obs_data_set_int(settings, "QP.PFrame", cqp);
	obs_data_set_int(settings, "QP.BFrame", cqp);
	obs_data_set_int(settings, "VBVBuffer", 1);
	obs_data_set_int(settings, "VBVBuffer.Size", 100000);

	// Picture Control Properties
	obs_data_set_double(settings, "KeyframeInterval", 2.0);
	obs_data_set_int(settings, "BFrame.Pattern", 0);

	// Update and release
	obs_encoder_update(s_videoRecordingEncoder.get(), settings);
	obs_data_release(settings);
}

void OBS_service::UpdateRecordingSettings_x264_crf(int crf)
{
	obs_data_t* settings = obs_data_create();
	obs_data_set_int(settings, "crf", crf);
	obs_data_set_bool(settings, "use_bufsize", true);
	obs_data_set_string(settings, "rate_control", "CRF");
	obs_data_set_string(settings, "profile", "high");
	obs_data_set_string(settings, "preset", s_lowCPUx264 ? "ultrafast" : "veryfast");

	obs_encoder_update(s_videoRecordingEncoder.get(), settings);

	obs_data_release(settings);
}

#define CROSS_DIST_CUTOFF 2000.0

int CalcCRF(int crf)
{
	std::string basicConfigFile = OBS_API::getBasicConfigPath();
	config_t*   config          = OBS_API::openConfigFile(basicConfigFile);

	int    cx  = config_get_uint(config, "Video", "OutputCX");
	int    cy  = config_get_uint(config, "Video", "OutputCY");
	double fCX = double(cx);
	double fCY = double(cy);

	if (s_lowCPUx264)
		crf -= 2;

	double crossDist       = sqrt(fCX * fCX + fCY * fCY);
	double crfResReduction = fmin(CROSS_DIST_CUTOFF, crossDist) / CROSS_DIST_CUTOFF;
	crfResReduction        = (1.0 - crfResReduction) * 10.0;

	return crf - int(crfResReduction);
}

void OBS_service::UpdateRecordingSettings()
{
	bool ultra_hq = (s_videoQuality == "HQ");
	int  crf      = CalcCRF(ultra_hq ? 16 : 23);

	if (astrcmp_n(s_videoEncoder.c_str(), "obs_x264", 4) == 0) {
		UpdateRecordingSettings_x264_crf(crf);

	} else if (s_videoEncoder == SIMPLE_ENCODER_QSV) {
		UpdateRecordingSettings_qsv11(crf);

	} else if (s_videoEncoder == SIMPLE_ENCODER_AMD) {
		UpdateRecordingSettings_amd_cqp(crf);

	} else if (s_videoEncoder == SIMPLE_ENCODER_NVENC) {
		UpdateRecordingSettings_nvenc(crf);
	}
}

std::shared_ptr<obs_encoder_t> OBS_service::getStreamingEncoder(void)
{
	if (!obs_initialized())
		throw "Trying to get an obs object but it isn't active";
	return s_videoStreamingEncoder;
}

void OBS_service::setStreamingEncoder(obs_encoder_t* encoder)
{
	s_videoStreamingEncoder = std::shared_ptr<obs_encoder_t>(encoder, default_delete<obs_encoder_t>());
}

std::shared_ptr<obs_encoder_t> OBS_service::getRecordingEncoder(void)
{
	if (!obs_initialized())
		throw "Trying to get an obs object but it isn't active";
	return s_videoRecordingEncoder;
}

void OBS_service::setRecordingEncoder(obs_encoder_t* encoder)
{
	s_videoRecordingEncoder = std::shared_ptr<obs_encoder_t>(encoder, default_delete<obs_encoder_t>());
}

std::shared_ptr<obs_encoder_t> OBS_service::getAudioStreamingEncoder(void)
{
	if (!obs_initialized())
		throw "Trying to get an obs object but it isn't active";
	return s_audioStreamingEncoder;
}

void OBS_service::setAudioStreamingEncoder(obs_encoder_t* encoder)
{
	s_audioStreamingEncoder = std::shared_ptr<obs_encoder_t>(encoder, default_delete<obs_encoder_t>());
}

std::shared_ptr<obs_encoder_t> OBS_service::getAudioRecordingEncoder(void)
{
	if (!obs_initialized())
		throw "Trying to get an obs object but it isn't active";
	return s_audioRecordingEncoder;
}

void OBS_service::setAudioRecordingEncoder(obs_encoder_t* encoder)
{
	s_audioRecordingEncoder = std::shared_ptr<obs_encoder_t>(encoder, default_delete<obs_encoder_t>());
}

std::shared_ptr<obs_output_t> OBS_service::getStreamingOutput(void)
{
	//if (!obs_initialized())
	//	throw "Trying to get an obs object but it isn't active";
	return s_streamingOutput;
}

void OBS_service::setStreamingOutput(obs_output_t* output)
{
	s_streamingOutput = std::shared_ptr<obs_output_t>(output, default_delete<obs_output_t>());
}

std::shared_ptr<obs_output_t> OBS_service::getRecordingOutput(void)
{
	// if (!obs_initialized())
		// throw "Trying to get an obs object but it isn't active";
	return s_recordingOutput;
}

void OBS_service::setRecordingOutput(obs_output_t* output)
{
	s_recordingOutput = std::shared_ptr<obs_output_t>(output, default_delete<obs_output_t>());
}

void OBS_service::updateStreamSettings(void)
{
	std::string basicConfigFile = OBS_API::getBasicConfigPath();
	config_t*   config          = OBS_API::openConfigFile(basicConfigFile);

	const char* currentOutputMode = config_get_string(config, "Output", "Mode");

	if (strcmp(currentOutputMode, "Simple") == 0) {
		const char* quality = config_get_string(config, "SimpleOutput", "RecQuality");
		if ((strcmp(quality, "Stream") != 0) || (strcmp(quality, "Stream") == 0 && !s_isRecording)) {
			updateVideoStreamingEncoder();
		}
	} else if (strcmp(currentOutputMode, "Advanced") == 0) {
		bool applyServiceSettings = config_get_bool(config, "AdvOut", "ApplyServiceSettings");

		if (applyServiceSettings) {
			obs_data_t* encoderSettings = obs_encoder_get_settings(s_videoStreamingEncoder.get());
			obs_service_apply_encoder_settings(OBS_service::getService().get(), encoderSettings, nullptr);
		}
	}

	bool useDelay      = config_get_bool(config, "Output", "DelayEnable");
	int  delaySec      = config_get_int(config, "Output", "DelaySec");
	bool preserveDelay = config_get_bool(config, "Output", "DelayPreserve");

	obs_output_set_delay(
	    s_streamingOutput.get(), useDelay ? delaySec : 0, preserveDelay ? OBS_OUTPUT_DELAY_PRESERVE : 0);

	associateAudioAndVideoToTheCurrentStreamingContext();
	associateAudioAndVideoEncodersToTheCurrentStreamingOutput();
}

void OBS_service::updateRecordSettings(void)
{
	std::string basicConfigFile = OBS_API::getBasicConfigPath();
	config_t*   config          = OBS_API::openConfigFile(basicConfigFile);

	const char* currentOutputMode = config_get_string(config, "Output", "Mode");

	if (strcmp(currentOutputMode, "Simple") == 0) {
		updateVideoRecordingEncoder();
		updateRecordingOutput();
	} else if (strcmp(currentOutputMode, "Advanced") == 0) {
		const char* recType = config_get_string(config, "AdvOut", "RecType");
		if (recType != NULL && strcmp(recType, "Custom Output (FFmpeg)") == 0) {
			resetVideoContext("Record");
			associateAudioAndVideoToTheCurrentRecordingContext();
			UpdateFFmpegOutput();
			return;
		}
		updateAdvancedRecordingOutput();
	}

	associateAudioAndVideoToTheCurrentRecordingContext();
	associateAudioAndVideoEncodersToTheCurrentRecordingOutput();
}

std::vector<SignalInfo> streamingSignals;
std::vector<SignalInfo> recordingSignals;

void OBS_service::OBS_service_connectOutputSignals(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	streamingSignals.push_back(SignalInfo("streaming", "start"));
	streamingSignals.push_back(SignalInfo("streaming", "stop"));
	streamingSignals.push_back(SignalInfo("streaming", "starting"));
	streamingSignals.push_back(SignalInfo("streaming", "stopping"));
	streamingSignals.push_back(SignalInfo("streaming", "activate"));
	streamingSignals.push_back(SignalInfo("streaming", "deactivate"));
	streamingSignals.push_back(SignalInfo("streaming", "reconnect"));
	streamingSignals.push_back(SignalInfo("streaming", "reconnect_success"));

	recordingSignals.push_back(SignalInfo("recording", "start"));
	recordingSignals.push_back(SignalInfo("recording", "stop"));
	recordingSignals.push_back(SignalInfo("recording", "stopping"));

	connectOutputSignals();

	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
}

std::mutex             signalMutex;
std::queue<SignalInfo> outputSignal;

void OBS_service::Query(
    void*                          data,
    const int64_t                  id,
    const std::vector<ipc::value>& args,
    std::vector<ipc::value>&       rval)
{
	std::unique_lock<std::mutex> ulock(signalMutex);
	if (outputSignal.empty()) {
		rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));
		AUTO_DEBUG;
		return;
	}

	rval.push_back(ipc::value((uint64_t)ErrorCode::Ok));

	rval.push_back(ipc::value(outputSignal.front().getOutputType()));
	rval.push_back(ipc::value(outputSignal.front().getSignal()));
	rval.push_back(ipc::value(outputSignal.front().getCode()));
	rval.push_back(ipc::value(outputSignal.front().getErrorMessage()));

	outputSignal.pop();

	AUTO_DEBUG;
}

void OBS_service::JSCallbackOutputSignal(void* data, calldata_t* params)
{
	SignalInfo& signal = *reinterpret_cast<SignalInfo*>(data);

	std::string signalReceived = signal.getSignal();

	if (signalReceived.compare("stop") == 0) {
		signal.setCode((int)calldata_int(params, "code"));

		obs_output_t* output;

		if (signal.getOutputType().compare("streaming") == 0)
			output = s_streamingOutput.get();
		else
			output = s_recordingOutput.get();

		const char* error = obs_output_get_last_error(output);
		if (error) {
			if (signal.getOutputType().compare("recording") == 0 && signal.getCode() == 0)
				signal.setCode(OBS_OUTPUT_ERROR);
			signal.setErrorMessage(error);
		}
	}

	std::unique_lock<std::mutex> ulock(signalMutex);
	outputSignal.push(signal);
}

void OBS_service::connectOutputSignals(void)
{
	signal_handler* streamingOutputSignalHandler = obs_output_get_signal_handler(s_streamingOutput.get());

	// Connect streaming output
	for (int i = 0; i < streamingSignals.size(); i++) {
		signal_handler_connect(
		    streamingOutputSignalHandler,
		    streamingSignals.at(i).getSignal().c_str(),
		    JSCallbackOutputSignal,
		    &(streamingSignals.at(i)));
	}

	signal_handler* recordingOutputSignalHandler = obs_output_get_signal_handler(s_recordingOutput.get());

	// Connect recording output
	for (int i = 0; i < recordingSignals.size(); i++) {
		signal_handler_connect(
		    recordingOutputSignalHandler,
		    recordingSignals.at(i).getSignal().c_str(),
		    JSCallbackOutputSignal,
		    &(recordingSignals.at(i)));
	}
}
