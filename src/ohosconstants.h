#pragma once
#include <utils/id.h>
namespace Ohos::Constants {
const char HARMONY_SETTINGS_ID[] = "LL.Harmony Configurations";
const char HARMONY_TOOLCHAIN_TYPEID[] = "Qt4ProjectManager.ToolChain.Harmony";
const char HARMONY_QT_TYPE[] = "Qt4ProjectManager.QtVersion.Harmony";
const char HARMONY_DEVICE_ID[] = "Harmony Device";
const char HARMONY_DEVICE_TYPE[] = "Harmony.Device.Type";
const char MakeLocationKey[] = "Harmony.MakeLocation";
const char DevecoStudioLocationKey[] = "Harmony.DevecoStudioPath";
const char QmakeLocationKey[] = "Harmony.QmakeLocations";
const char SDKLocationsKey[] = "Harmony.SDKLocations";
const char DefaultSDKLocationKey[] = "Harmony.DefaultSDKLocation";
const char OHOS_SDK_ENV_VAR[] = "OHOS_SDK_PATH";
const char OHOS_SDK_VERSION[] = "OHOS_SDK_VERSION";
const char OHOS_ARCH[] = "OHOS_ARCH";
const char OHOS_TOOLCHAIN_FILE[] = "ohos.toolchain.cmake";
const char Q_CONFIG_H[] = "QtCore/qconfig.h";
const char Q_DEVICE_PRI[] = "qdevice.pri";

const char HARMONY_KIT_NDK[] = "Harmony.NDK";
const char HARMONY_KIT_SDK[] = "Harmony.SDK";

const char HarmonySerialNumber[] = "HarmonySerialNumber";
const char HarmonyCpuAbi[] = "HarmonyCpuAbi";
const char HarmonySdk[] = "HarmonySdk";
namespace Parameter {
    namespace Product
    {
        const char productid[] = "const.product.productid";
        const char name[] = "const.product.name";
        namespace Dfx
        {
            namespace Fans
            {
                const char stage[] = "const.product.dfx.fans.stage";
            }
        }
        namespace Os
        {
            namespace Dist
            {
                const char apiname[] = "const.product.os.dist.apiname";
                const char version[] = "const.product.os.dist.version";
                const char apiversion[] = "const.product.os.dist.apiversion";
                const char releasetype[] = "const.product.os.dist.releasetype";
            }
        }
        namespace Cpu
        {
            const char abilist[] = "const.product.cpu.abilist";
        }
        const char model[] = "const.product.model";
        const char hide[] = "const.product.hide";
        namespace Hide
        {
            const char matchers[] = "const.product.hide.matchers";
            const char replacements[] = "const.product.hide.replacements";
        }
        const char brand[] = "const.product.brand";
        namespace Build
        {
            const char type[] = "const.product.build.type";
            const char user[] = "const.product.build.user";
            const char host[] = "const.product.build.host";
            const char date[] = "const.product.build.date";
        }
        const char software[] = "const.product.software.version";
        const char cover_mode[] = "const.product.cover_mode";
        const char baseappid[] = "const.product.baseappid";
        const char device_authentication[] = "const.product.device_authentication";
        const char device_radius[] = "const.product.device_radius";
        const char manufacturer[] = "const.product.manufacturer";
        namespace Bootloader
        {
            const char version[] = "const.product.bootloader.version";
        }
        namespace Incremental
        {
            const char version[] = "const.product.incremental.version";
        }
        const char firstapiversion[] = "const.product.firstapiversion";
        const char hardwareversion[] = "const.product.hardwareversion";
        const char hardwareprofile[] = "const.product.hardwareprofile";
        const char devicetype[] = "const.product.devicetype";
    }
    namespace Ohos
    {
        namespace Version
        {
            const char security_patch[] = "const.ohos.version.security_patch";
            const char certified[] = "const.ohos.version.certified";
        }
        const char releasetype[] = "const.ohos.releasetype";
        const char apiversion[] = "const.ohos.apiversion";
        const char fullname[] = "const.ohos.fullname";
        const char buildroothash[] = "const.ohos.buildroothash";
    }
    namespace Build
    {
        const char description[] = "const.build.description";
        const char product[] = "const.build.product";
        const char vendor_date[] = "const.build.vendor.date";
        const char vendor_date_utc[] = "const.build.vendor.date.utc";
        const char ver_physical[] = "const.build.ver.physical";
        const char system_date[] = "const.build.system.date";
        const char system_date_utc[] = "const.build.system.date.utc";
        const char sa_sdk_version[] = "const.build.sa_sdk_version";
        const char characteristics[] = "const.build.characteristics";
    }
    namespace SystemCapability
    {
        namespace AI
        {
            const char AICaption[] = "const.SystemCapability.AI.AICaption";
            const char AiEngine[] = "const.SystemCapability.AI.AiEngine";
            namespace Face
            {
                const char Comparator[] = "const.SystemCapability.AI.Face.Comparator";
                const char Detector[] = "const.SystemCapability.AI.Face.Detector";
            }
            namespace OCR
            {
                const char TextRecognition[] = "const.SystemCapability.AI.OCR.TextRecognition";
            }
            const char Search[] = "const.SystemCapability.AI.Search";
            namespace Vision
            {
                const char ObjectDetection[] = "const.SystemCapability.AI.Vision.ObjectDetection";
                const char VisionBase[] = "const.SystemCapability.AI.Vision.VisionBase";
                const char SkeletonDetection[] = "const.SystemCapability.AI.Vision.SkeletonDetection";
                const char SubjectSegmentation[] = "const.SystemCapability.AI.Vision.SubjectSegmentation";
            }
            namespace Component
            {
                const char CardRecognition[] = "const.SystemCapability.AI.Component.CardRecognition";
                const char DocScan[] = "const.SystemCapability.AI.Component.DocScan";
                const char LivenessDetect[] = "const.SystemCapability.AI.Component.LivenessDetect";
                const char TextReader[] = "const.SystemCapability.AI.Component.TextReader";
            }
            const char GenerateAI[] = "const.SystemCapability.AI.GenerateAI.LLM";
            const char HiAIFoundation[] = "const.SystemCapability.AI.HiAIFoundation";
            const char InsightIntent[] = "const.SystemCapability.AI.InsightIntent";
            const char MindSporeLite[] = "const.SystemCapability.AI.MindSporeLite";
            const char TextToSpeech[] = "const.SystemCapability.AI.TextToSpeech";
            namespace IntelligentKws
            {
                const char Core[] = "const.SystemCapability.AI.IntelligentKws.Core";
            }
            const char ImageAnalyzerOverlay[] = "const.SystemCapability.AI.ImageAnalyzerOverlay";
            namespace IntelligentVoice
            {
                const char Core[] = "const.SystemCapability.AI.IntelligentVoice.Core";
            }
            namespace NaturalLanguage
            {
                const char TextProcessing[] = "const.SystemCapability.AI.NaturalLanguage.TextProcessing";
            }
            const char SpeechRecognizer[] = "const.SystemCapability.AI.SpeechRecognizer";
            const char NeuralNetworkRuntime[] = "const.SystemCapability.AI.NeuralNetworkRuntime";
            const char VisionImageAnalyzer[] = "const.SystemCapability.AI.VisionImageAnalyzer";
            const char MindSpore[] = "const.SystemCapability.Ai.MindSpore";
        }
        namespace AREngine
        {
            const char Core[] = "const.SystemCapability.AREngine.Core";
        }
        namespace Ability
        {
            const char AbilityBase[] = "const.SystemCapability.Ability.AbilityBase";
            const char AppStartup[] = "const.SystemCapability.Ability.AppStartup";
            const char Form[] = "const.SystemCapability.Ability.Form";
            namespace AbilityRuntime
            {
                const char AbilityCore[] = "const.SystemCapability.Ability.AbilityRuntime.AbilityCore";
                const char Core[] = "const.SystemCapability.Ability.AbilityRuntime.Core";
                const char FAModel[] = "const.SystemCapability.Ability.AbilityRuntime.FAModel";
                const char Mission[] = "const.SystemCapability.Ability.AbilityRuntime.Mission";
                const char QuickFix[] = "const.SystemCapability.Ability.AbilityRuntime.QuickFix";
            }
            namespace AbilityTools
            {
                const char AbilityAssistant[] = "const.SystemCapability.Ability.AbilityTools.AbilityAssistant";
            }
            namespace AppExtension
            {
                const char PhotoEditorExtension[] = "const.SystemCapability.Ability.AppExtension.PhotoEditorExtension";
            }
            const char DistributedAbilityManager[] = "const.SystemCapability.Ability.DistributedAbilityManager";
        }
        namespace Account
        {
            const char AppAccount[] = "const.SystemCapability.Account.AppAccount";
            const char OsAccount[] = "const.SystemCapability.Account.OsAccount";
        }
        namespace ArkUI
        {
            const char ArkUIFull[] = "const.SystemCapability.ArkUI.ArkUI.Full";
            const char ArkUILite[] = "const.SystemCapability.ArkUI.ArkUI.Lite";
            const char ArkUINapi[] = "const.SystemCapability.ArkUI.ArkUI.Napi";
            const char ArkUILibuv[] = "const.SystemCapability.ArkUI.ArkUI.Libuv";
            const char UiAppearance[] = "const.SystemCapability.ArkUI.UiAppearance";
            const char Graphics3D[] = "const.SystemCapability.ArkUi.Graphics3D";
        }
        const char Base[] = "const.SystemCapability.Base";
        namespace Game
        {
            namespace GameService
            {
                const char GamePlayer[] = "const.SystemCapability.Game.GameService.GamePlayer";
            }
        }
        namespace Map
        {
            const char Core[] = "const.SystemCapability.Map.Core";
        }
        namespace USB
        {
            const char USBManager[] = "const.SystemCapability.USB.USBManager";
        }
        namespace Web
        {
            namespace Webview
            {
                const char Core[] = "const.SystemCapability.Web.Webview.Core";
            }
        }
        namespace XTS
        {
            const char DeviceAttest[] = "const.SystemCapability.XTS.DeviceAttest";
        }
        namespace Msdp
        {
            namespace DeviceStatus
            {
                const char Cooperate[] = "const.SystemCapability.Msdp.DeviceStatus.Cooperate";
                const char Drag[] = "const.SystemCapability.Msdp.DeviceStatus.Drag";
                const char Stationary[] = "const.SystemCapability.Msdp.DeviceStatus.Stationary";
            }
            const char Geofence[] = "const.SystemCapability.Msdp.Geofence";
            const char Motion[] = "const.SystemCapability.Msdp.Motion";
            const char Movement[] = "const.SystemCapability.Msdp.Movement";
            const char Timeline[] = "const.SystemCapability.Msdp.Timeline";
            const char MultiModalAwareness[] = "const.SystemCapability.Msdp.MultiModalAwareness";
            const char SpatialAwareness[] = "const.SystemCapability.Msdp.SpatialAwareness";
            const char UserStatusAwareness[] = "const.SystemCapability.Msdp.UserStatusAwareness";
        }
        namespace Push
        {
            const char PushService[] = "const.SystemCapability.Push.PushService";
        }
        namespace Test
        {
            const char UiTest[] = "const.SystemCapability.Test.UiTest";
        }
        namespace Cloud
        {
            const char HiAnalytics[] = "const.SystemCapability.Cloud.HiAnalytics";
        }
        namespace Print
        {
            const char PrintFramework[] = "const.SystemCapability.Print.PrintFramework";
        }
        namespace Utils
        {
            const char Lang[] = "const.SystemCapability.Utils.Lang";
        }
        namespace Driver
        {
            const char DDKExtension[] = "const.SystemCapability.Driver.DDK.Extension";
            const char HIDExtension[] = "const.SystemCapability.Driver.HID.Extension";
            const char USBExtension[] = "const.SystemCapability.Driver.USB.Extension";
            const char ExternalDevice[] = "const.SystemCapability.Driver.ExternalDevice";
        }
        namespace Global
        {
            const char I18n[] = "const.SystemCapability.Global.I18n";
            const char I18nExt[] = "const.SystemCapability.Global.I18nExt";
            const char ResourceManager[] = "const.SystemCapability.Global.ResourceManager";
        }
        namespace Health
        {
            const char Cooperation[] = "const.SystemCapability.Health.Cooperation";
            const char HealthStore[] = "const.SystemCapability.Health.HealthStore";
            const char WearEngine[] = "const.SystemCapability.Health.WearEngine";
            const char HealthService[] = "const.SystemCapability.Health.HealthService";
        }
        namespace Stylus
        {
            const char ColorPicker[] = "const.SystemCapability.Stylus.ColorPicker";
            const char Handwrite[] = "const.SystemCapability.Stylus.Handwrite";
            const char StylusService[] = "const.SystemCapability.Stylus.StylusService";
        }
        namespace Update
        {
            const char DistributedUpdateEngine[] = "const.SystemCapability.Update.DistributedUpdateEngine";
            const char UpdateService[] = "const.SystemCapability.Update.UpdateService";
        }
        namespace Window
        {
            const char SessionManager[] = "const.SystemCapability.Window.SessionManager";
        }
        namespace Graphic
        {
            namespace ApsManager
            {
                const char Resolution[] = "const.SystemCapability.Graphic.ApsManager.Resolution";
            }
            namespace Graphic2D
            {
                const char ColorManagerCore[] = "const.SystemCapability.Graphic.Graphic2D.ColorManager.Core";
                const char EGL[] = "const.SystemCapability.Graphic.Graphic2D.EGL";
                const char GLES2[] = "const.SystemCapability.Graphic.Graphic2D.GLES2";
                const char GLES3[] = "const.SystemCapability.Graphic.Graphic2D.GLES3";
                const char WebGL[] = "const.SystemCapability.Graphic.Graphic2D.WebGL";
                const char NativeBuffer[] = "const.SystemCapability.Graphic.Graphic2D.NativeBuffer";
                const char NativeImage[] = "const.SystemCapability.Graphic.Graphic2D.NativeImage";
                const char NativeVsync[] = "const.SystemCapability.Graphic.Graphic2D.NativeVsync";
                const char WebGL2[] = "const.SystemCapability.Graphic.Graphic2D.WebGL2";
                const char NativeWindow[] = "const.SystemCapability.Graphic.Graphic2D.NativeWindow";
                const char HyperGraphicManager[] = "const.SystemCapability.Graphic.Graphic2D.HyperGraphicManager";
                const char NativeDrawing[] = "const.SystemCapability.Graphic.Graphic2D.NativeDrawing";
            }
            const char Vulkan[] = "const.SystemCapability.Graphic.Vulkan";
        }
        namespace Payment
        {
            const char ECNYPaymentService[] = "const.SystemCapability.Payment.ECNYPaymentService";
            const char IAP[] = "const.SystemCapability.Payment.IAP";
            const char PaymentService[] = "const.SystemCapability.Payment.PaymentService";
        }
        namespace Request
        {
            const char FileTransferAgent[] = "const.SystemCapability.Request.FileTransferAgent";
        }
        namespace Sensors
        {
            const char MiscDevice[] = "const.SystemCapability.Sensors.MiscDevice";
            const char MiscDeviceLite[] = "const.SystemCapability.Sensors.MiscDevice.Lite";
            const char Sensor[] = "const.SystemCapability.Sensors.Sensor";
            const char SensorLite[] = "const.SystemCapability.Sensors.Sensor.Lite";
        }
        namespace Startup
        {
            const char SystemInfo[] = "const.SystemCapability.Startup.SystemInfo";
            const char SystemInfoLite[] = "const.SystemCapability.Startup.SystemInfo.Lite";
        }
        namespace UserIAM
        {
            const char FingerprintAuthExt[] = "const.SystemCapability.UserIAM.FingerprintAuthExt";
            const char UserAuthCore[] = "const.SystemCapability.UserIAM.UserAuth.Core";
            const char UserAuthFaceAuth[] = "const.SystemCapability.UserIAM.UserAuth.FaceAuth";
            const char UserAuthPinAuth[] = "const.SystemCapability.UserIAM.UserAuth.PinAuth";
            const char UserAuthFingerprintAuth[] = "const.SystemCapability.UserIAM.UserAuth.FingerprintAuth";
        }
        namespace Weather
        {
            const char Core[] = "const.SystemCapability.Weather.Core";
        }
        namespace Graphics
        {
            const char Drawing[] = "const.SystemCapability.Graphics.Drawing";
        }
        namespace HuaweiID
        {
            const char InvoiceAssistant[] = "const.SystemCapability.HuaweiID.InvoiceAssistant";
        }
        namespace LiveView
        {
            const char LiveViewService[] = "const.SystemCapability.LiveView.LiveViewService";
        }
        namespace Location
        {
            const char LocationCore[] = "const.SystemCapability.Location.Location.Core";
            const char LocationGnss[] = "const.SystemCapability.Location.Location.Gnss";
            const char LocationLite[] = "const.SystemCapability.Location.Location.Lite";
            const char LocationGeocoder[] = "const.SystemCapability.Location.Location.Geocoder";
            const char LocationGeofence[] = "const.SystemCapability.Location.Location.Geofence";
        }
        namespace Ringtone
        {
            const char Core[] = "const.SystemCapability.Ringtone.Core";
        }
        namespace Security
        {
            const char AccessToken[] = "const.SystemCapability.Security.AccessToken";
            const char Asset[] = "const.SystemCapability.Security.Asset";
            const char Cert[] = "const.SystemCapability.Security.Cert";
            const char FIDO[] = "const.SystemCapability.Security.FIDO";
            const char HuksAttestKeyExt[] = "const.SystemCapability.Security.Huks.AttestKeyExt";
            const char HuksCore[] = "const.SystemCapability.Security.Huks.Core";
            const char HuksExtension[] = "const.SystemCapability.Security.Huks.Extension";
            const char Ifaa[] = "const.SystemCapability.Security.Ifaa";
            const char SOTER[] = "const.SystemCapability.Security.SOTER";
            const char Cipher[] = "const.SystemCapability.Security.Cipher";
            const char CodeProtect[] = "const.SystemCapability.Security.CodeProtect";
            const char DeviceAuth[] = "const.SystemCapability.Security.DeviceAuth";
            const char TrustedRing[] = "const.SystemCapability.Security.TrustedRing";
            const char ActivationLock[] = "const.SystemCapability.Security.ActivationLock";
            const char PrivateSpace[] = "const.SystemCapability.Security.PrivateSpace";
            const char SafetyDetect[] = "const.SystemCapability.Security.SafetyDetect";
            const char SecurityGuard[] = "const.SystemCapability.Security.SecurityGuard";
            const char BusinessRiskIntelligentDetection[] = "const.SystemCapability.Security.BusinessRiskIntelligentDetection";
            const char CertificateManager[] = "const.SystemCapability.Security.CertificateManager";
            const char cryptoFramework[] = "const.SystemCapability.Security.CryptoFramework";
            namespace CryptoFramework
            {
                const char Cipher[] = "const.SystemCapability.Security.CryptoFramework.Cipher";
                const char Kdf[] = "const.SystemCapability.Security.CryptoFramework.Kdf";
                const char key[] = "const.SystemCapability.Security.CryptoFramework.Key";
                namespace Key
                {
                    const char AsymKey[] = "const.SystemCapability.Security.CryptoFramework.Key.AsymKey";
                    const char SymKey[] = "const.SystemCapability.Security.CryptoFramework.Key.SymKey";
                }
                const char Mac[] = "const.SystemCapability.Security.CryptoFramework.Mac";
                const char Rand[] = "const.SystemCapability.Security.CryptoFramework.Rand";
                const char KeyAgreement[] = "const.SystemCapability.Security.CryptoFramework.KeyAgreement";
                const char Signature[] = "const.SystemCapability.Security.CryptoFramework.Signature";
                const char MessageDigest[] = "const.SystemCapability.Security.CryptoFramework.MessageDigest";
            }
            const char DataLossPrevention[] = "const.SystemCapability.Security.DataLossPrevention";
            const char DataTransitManager[] = "const.SystemCapability.Security.DataTransitManager";
            const char DeviceCertificate[] = "const.SystemCapability.Security.DeviceCertificate";
            const char trustedAppService[] = "const.SystemCapability.Security.TrustedAppService";
            namespace TrustedAppService
            {
                const char Core[] = "const.SystemCapability.Security.TrustedAppService.Core";
                const char Location[] = "const.SystemCapability.Security.TrustedAppService.Location";
            }
            const char DeviceSecurityMode[] = "const.SystemCapability.Security.DeviceSecurityMode";
            const char DeviceHealthAttestation[] = "const.SystemCapability.Security.DeviceHealthAttestation";
            const char DeviceSecurityLevel[] = "const.SystemCapability.Security.DeviceSecurityLevel";
            const char DlpCredentialService[] = "const.SystemCapability.Security.DlpCredentialService";
            const char ScreenLockFileManager[] = "const.SystemCapability.Security.ScreenLockFileManager";
            const char SecurityPrivacyServer[] = "const.SystemCapability.Security.SecurityPrivacyServer";
        }
        namespace UIDesign
        {
            const char Core[] = "const.SystemCapability.UIDesign.Core";
        }
        namespace Advertising
        {
            const char Ads[] = "const.SystemCapability.Advertising.Ads";
            const char OAID[] = "const.SystemCapability.Advertising.OAID";
        }
        namespace ArkCompiler
        {
            const char JSVM[] = "const.SystemCapability.ArkCompiler.JSVM";
        }
        namespace BarrierFree
        {
            namespace Accessibility
            {
                const char Core[] = "const.SystemCapability.BarrierFree.Accessibility.Core";
                const char Hearing[] = "const.SystemCapability.BarrierFree.Accessibility.Hearing";
                const char Vision[] = "const.SystemCapability.BarrierFree.Accessibility.Vision";
            }
        }
        namespace CarService
        {
            const char DistributedEngine[] = "const.SystemCapability.CarService.DistributedEngine";
            const char NavigationInfo[] = "const.SystemCapability.CarService.NavigationInfo";
        }
        namespace FindDevice
        {
            const char FindNetwork[] = "const.SystemCapability.FindDevice.FindNetwork";
        }
        namespace HiViewDFX
        {
            const char HiviewcareManager[] = "const.SystemCapability.HiViewDFX.HiviewcareManager";
            const char Maintenance[] = "const.SystemCapability.HiViewDFX.Maintenance";
            const char HiAppEvent[] = "const.SystemCapability.HiViewDFX.HiAppEvent";
            const char HiChecker[] = "const.SystemCapability.HiViewDFX.HiChecker";
            const char HiCollie[] = "const.SystemCapability.HiViewDFX.HiCollie";
            const char HiDumper[] = "const.SystemCapability.HiViewDFX.HiDumper";
            const char HiLog[] = "const.SystemCapability.HiViewDFX.HiLog";
            const char HiTrace[] = "const.SystemCapability.HiViewDFX.HiTrace";
            const char ChrLogService[] = "const.SystemCapability.HiViewDFX.HiView.ChrLogService";
            const char LogService[] = "const.SystemCapability.HiViewDFX.HiView.LogService";
            const char Hiview[] = "const.SystemCapability.HiViewDFX.Hiview";
            const char FaultLogger[] = "const.SystemCapability.HiViewDFX.Hiview.FaultLogger";
            const char LogLibrary[] = "const.SystemCapability.HiViewDFX.Hiview.LogLibrary";
            const char XPower[] = "const.SystemCapability.HiViewDFX.XPower";
            const char InfoSec[] = "const.SystemCapability.HiViewDFX.InfoSec";
            const char HiProfiler[] = "const.SystemCapability.HiViewDFX.HiProfiler.HiDebug";
            const char HiSysEvent[] = "const.SystemCapability.HiViewDFX.HiSysEvent";
        }
        namespace PCService
        {
            const char StatusBarManager[] = "const.SystemCapability.PCService.StatusBarManager";
        }
        namespace Telephony
        {
            const char CallManager[] = "const.SystemCapability.Telephony.CallManager";
            const char CoreService[] = "const.SystemCapability.Telephony.CoreService";
            const char SmsMms[] = "const.SystemCapability.Telephony.SmsMms";
            const char CellularCall[] = "const.SystemCapability.Telephony.CellularCall";
            const char CellularData[] = "const.SystemCapability.Telephony.CellularData";
            const char StateRegistry[] = "const.SystemCapability.Telephony.StateRegistry";
            const char TelephonyEnhanced[] = "const.SystemCapability.Telephony.TelephonyEnhanced";
            const char VoipCallManager[] = "const.SystemCapability.Telephony.VoipCallManager";
        }
        namespace Multimedia
        {
            namespace AVSession
            {
                const char AVCast[] = "const.SystemCapability.Multimedia.AVSession.AVCast";
                const char Core[] = "const.SystemCapability.Multimedia.AVSession.Core";
                const char ExtendedDisplayCast[] = "const.SystemCapability.Multimedia.AVSession.ExtendedDisplayCast";
                const char Manager[] = "const.SystemCapability.Multimedia.AVSession.Manager";
            }
            namespace Audio
            {
                const char Capturer[] = "const.SystemCapability.Multimedia.Audio.Capturer";
                const char Core[] = "const.SystemCapability.Multimedia.Audio.Core";
                const char Tone[] = "const.SystemCapability.Multimedia.Audio.Tone";
                const char Device[] = "const.SystemCapability.Multimedia.Audio.Device";
                const char Volume[] = "const.SystemCapability.Multimedia.Audio.Volume";
                const char Renderer[] = "const.SystemCapability.Multimedia.Audio.Renderer";
                const char Communication[] = "const.SystemCapability.Multimedia.Audio.Communication";
                const char Interrupt[] = "const.SystemCapability.Multimedia.Audio.Interrupt";
                const char PlaybackCapture[] = "const.SystemCapability.Multimedia.Audio.PlaybackCapture";
                const char Spatialization[] = "const.SystemCapability.Multimedia.Audio.Spatialization";
            }
            namespace Drm
            {
                const char Core[] = "const.SystemCapability.Multimedia.Drm.Core";
            }
            namespace Image
            {
                const char Core[] = "const.SystemCapability.Multimedia.Image.Core";
                const char ImageCreator[] = "const.SystemCapability.Multimedia.Image.ImageCreator";
                const char ImagePacker[] = "const.SystemCapability.Multimedia.Image.ImagePacker";
                const char ImageSource[] = "const.SystemCapability.Multimedia.Image.ImageSource";
                const char ImageReceiver[] = "const.SystemCapability.Multimedia.Image.ImageReceiver";
            }
            namespace Media
            {
                const char AVImageGenerator[] = "const.SystemCapability.Multimedia.Media.AVImageGenerator";
                const char AVPlayer[] = "const.SystemCapability.Multimedia.Media.AVPlayer";
                const char Core[] = "const.SystemCapability.Multimedia.Media.Core";
                const char Muxer[] = "const.SystemCapability.Multimedia.Media.Muxer";
                const char Spliter[] = "const.SystemCapability.Multimedia.Media.Spliter";
                const char AVRecorder[] = "const.SystemCapability.Multimedia.Media.AVRecorder";
                const char AudioCodec[] = "const.SystemCapability.Multimedia.Media.AudioCodec";
                const char CodecBase[] = "const.SystemCapability.Multimedia.Media.CodecBase";
                const char SoundPool[] = "const.SystemCapability.Multimedia.Media.SoundPool";
                const char AVScreenCapture[] = "const.SystemCapability.Multimedia.Media.AVScreenCapture";
                const char AVTransCoder[] = "const.SystemCapability.Multimedia.Media.AVTransCoder";
                const char AudioDecoder[] = "const.SystemCapability.Multimedia.Media.AudioDecoder";
                const char AudioEncoder[] = "const.SystemCapability.Multimedia.Media.AudioEncoder";
                const char AudioPlayer[] = "const.SystemCapability.Multimedia.Media.AudioPlayer";
                const char VideoPlayer[] = "const.SystemCapability.Multimedia.Media.VideoPlayer";
                const char VideoDecoder[] = "const.SystemCapability.Multimedia.Media.VideoDecoder";
                const char VideoEncoder[] = "const.SystemCapability.Multimedia.Media.VideoEncoder";
                const char AudioRecorder[] = "const.SystemCapability.Multimedia.Media.AudioRecorder";
                const char VideoRecorder[] = "const.SystemCapability.Multimedia.Media.VideoRecorder";
                const char AVMetadataExtractor[] = "const.SystemCapability.Multimedia.Media.AVMetadataExtractor";
            }
            namespace Scan
            {
                const char Core[] = "const.SystemCapability.Multimedia.Scan.Core";
                const char GenerateBarcode[] = "const.SystemCapability.Multimedia.Scan.GenerateBarcode";
                const char ScanBarcode[] = "const.SystemCapability.Multimedia.Scan.ScanBarcode";
            }
            namespace Camera
            {
                const char Core[] = "const.SystemCapability.Multimedia.Camera.Core";
            }
            namespace AudioHaptic
            {
                const char Core[] = "const.SystemCapability.Multimedia.AudioHaptic.Core";
            }
            namespace ImageEffect
            {
                const char Core[] = "const.SystemCapability.Multimedia.ImageEffect.Core";
            }
            namespace ImageLoader
            {
                const char Core[] = "const.SystemCapability.Multimedia.ImageLoader.Core";
            }
            namespace SystemSound
            {
                const char Core[] = "const.SystemCapability.Multimedia.SystemSound.Core";
            }
            namespace MediaLibrary
            {
                const char Core[] = "const.SystemCapability.Multimedia.MediaLibrary.Core";
                const char DistributedCore[] = "const.SystemCapability.Multimedia.MediaLibrary.DistributedCore";
            }
            const char VideoProcessingEngine[] = "const.SystemCapability.Multimedia.VideoProcessingEngine";
        }
        namespace GameService
        {
            const char GamePerformance[] = "const.SystemCapability.GameService.GamePerformance";
        }
        namespace VirtService
        {
            const char Base[] = "const.SystemCapability.VirtService.Base";
        }
        namespace AppGalleryService
        {
            const char AppInfoManager[] = "const.SystemCapability.AppGalleryService.AppInfoManager";
            const char DistributionOnDemandInstall[] = "const.SystemCapability.AppGalleryService.Distribution.OnDemandInstall";
            const char DistributionRecommendations[] = "const.SystemCapability.AppGalleryService.Distribution.Recommendations";
            const char DistributionUnifiedInstall[] = "const.SystemCapability.AppGalleryService.Distribution.UnifiedInstall";
            const char DistributionUpdate[] = "const.SystemCapability.AppGalleryService.Distribution.Update";
            const char PrivacyManager[] = "const.SystemCapability.AppGalleryService.PrivacyManager";
            const char AttributionManager[] = "const.SystemCapability.AppGalleryService.AttributionManager";
        }
        namespace Applications
        {
            const char CalendarData[] = "const.SystemCapability.Applications.CalendarData";
            const char Contacts[] = "const.SystemCapability.Applications.Contacts";
            const char SettingsCore[] = "const.SystemCapability.Applications.Settings.Core";
            const char ContactsData[] = "const.SystemCapability.Applications.ContactsData";
        }
        namespace Developtools
        {
            const char Syscap[] = "const.SystemCapability.Developtools.Syscap";
        }
        namespace GraphicsGame
        {
            const char RenderAccelerate[] = "const.SystemCapability.GraphicsGame.RenderAccelerate";
        }
        namespace MiscServices
        {
            const char Download[] = "const.SystemCapability.MiscServices.Download";
            const char Theme[] = "const.SystemCapability.MiscServices.Theme";
            const char Time[] = "const.SystemCapability.MiscServices.Time";
            const char Upload[] = "const.SystemCapability.MiscServices.Upload";
            const char InputMethodFramework[] = "const.SystemCapability.MiscServices.InputMethodFramework";
            const char Pasteboard[] = "const.SystemCapability.MiscServices.Pasteboard";
            const char ScreenLock[] = "const.SystemCapability.MiscServices.ScreenLock";
            const char Wallpaper[] = "const.SystemCapability.MiscServices.Wallpaper";
        }
        namespace Notification
        {
            const char CommonEvent[] = "const.SystemCapability.Notification.CommonEvent";
            const char Emitter[] = "const.SystemCapability.Notification.Emitter";
            const char Notification[] = "const.SystemCapability.Notification.Notification";
            const char NotificationSettings[] = "const.SystemCapability.Notification.NotificationSettings";
            const char ReminderAgent[] = "const.SystemCapability.Notification.ReminderAgent";
        }
        namespace PowerManager
        {
            namespace BatteryManager
            {
                const char Core[] = "const.SystemCapability.PowerManager.BatteryManager.Core";
                const char Extension[] = "const.SystemCapability.PowerManager.BatteryManager.Extension";
            }
            namespace PowerManager
            {
                const char Core[] = "const.SystemCapability.PowerManager.PowerManager.Core";
                const char Extension[] = "const.SystemCapability.PowerManager.PowerManager.Extension";
            }
            const char ThermalManager[] = "const.SystemCapability.PowerManager.ThermalManager";
            const char BatteryStatistics[] = "const.SystemCapability.PowerManager.BatteryStatistics";
            const char DisplayPowerManager[] = "const.SystemCapability.PowerManager.DisplayPowerManager";
            const char DisplayPowerManagerLite[] = "const.SystemCapability.PowerManager.DisplayPowerManager.Lite";
        }
        namespace BundleManager
        {
            const char AppDomainVerify[] = "const.SystemCapability.BundleManager.AppDomainVerify";
            const char bundleFramework[] = "const.SystemCapability.BundleManager.BundleFramework";
            namespace BundleFramework
            {
                const char AppControl[] = "const.SystemCapability.BundleManager.BundleFramework.AppControl";
                const char Core[] = "const.SystemCapability.BundleManager.BundleFramework.Core";
                const char DefaultApp[] = "const.SystemCapability.BundleManager.BundleFramework.DefaultApp";
                const char Launcher[] = "const.SystemCapability.BundleManager.BundleFramework.Launcher";
                const char Overlay[] = "const.SystemCapability.BundleManager.BundleFramework.Overlay";
                const char Resource[] = "const.SystemCapability.BundleManager.BundleFramework.Resource";
                const char FreeInstall[] = "const.SystemCapability.BundleManager.BundleFramework.FreeInstall";
            }
            const char Zlib[] = "const.SystemCapability.BundleManager.Zlib";
            const char DistributedBundleFramework[] = "const.SystemCapability.BundleManager.DistributedBundleFramework";
            namespace EcologicalRuleManager
            {
                const char EcologicalRuleDataManager[] = "const.SystemCapability.BundleManager.EcologicalRuleManager.EcologicalRuleDataManager";
                const char SceneManager[] = "const.SystemCapability.BundleManager.EcologicalRuleManager.SceneManager";
            }
        }
        namespace Collaboration
        {
            const char Camera[] = "const.SystemCapability.Collaboration.Camera";
            const char DevicePicker[] = "const.SystemCapability.Collaboration.DevicePicker";
            const char HarmonyShare[] = "const.SystemCapability.Collaboration.HarmonyShare";
            const char Service[] = "const.SystemCapability.Collaboration.Service";
            const char SystemShare[] = "const.SystemCapability.Collaboration.SystemShare";
            const char RemoteCommunication[] = "const.SystemCapability.Collaboration.RemoteCommunication";
            const char ServiceManager[] = "const.SystemCapability.Collaboration.ServiceManager";
        }
        namespace Communication
        {
            namespace Bluetooth
            {
                const char Core[] = "const.SystemCapability.Communication.Bluetooth.Core";
                const char Lite[] = "const.SystemCapability.Communication.Bluetooth.Lite";
            }
            namespace IPC
            {
                const char Core[] = "const.SystemCapability.Communication.IPC.Core";
            }
            namespace NearLink
            {
                const char Core[] = "const.SystemCapability.Communication.NearLink.Core";
            }
            const char NetStack[] = "const.SystemCapability.Communication.NetStack";
            const char SoftBusCore[] = "const.SystemCapability.Communication.SoftBus.Core";
            namespace WiFi
            {
                const char APCore[] = "const.SystemCapability.Communication.WiFi.AP.Core";
                const char Core[] = "const.SystemCapability.Communication.WiFi.Core";
                const char P2P[] = "const.SystemCapability.Communication.WiFi.P2P";
                const char STA[] = "const.SystemCapability.Communication.WiFi.STA";
            }
            namespace NetManager
            {
                const char Core[] = "const.SystemCapability.Communication.NetManager.Core";
                const char MDNS[] = "const.SystemCapability.Communication.NetManager.MDNS";
                const char Vpn[] = "const.SystemCapability.Communication.NetManager.Vpn";
                const char Ethernet[] = "const.SystemCapability.Communication.NetManager.Ethernet";
                const char NetFirewall[] = "const.SystemCapability.Communication.NetManager.NetFirewall";
                const char NetSharing[] = "const.SystemCapability.Communication.NetManager.NetSharing";
                const char SmartHotSpot[] = "const.SystemCapability.Communication.NetManager.SmartHotSpot";
            }
            const char NetworkBoostCore[] = "const.SystemCapability.Communication.NetworkBoost.Core";
        }
        namespace Customization
        {
            const char ConfigPolicy[] = "const.SystemCapability.Customization.ConfigPolicy";
            const char CustomConfig[] = "const.SystemCapability.Customization.CustomConfig";
            const char EnterpriseDeviceManager[] = "const.SystemCapability.Customization.EnterpriseDeviceManager";
            const char EnterpriseDeviceManagerExt[] = "const.SystemCapability.Customization.EnterpriseDeviceManagerExt";
        }
        namespace OfficeService
        {
            const char PDFServiceCore[] = "const.SystemCapability.OfficeService.PDFService.Core";
        }
        namespace WindowManager
        {
            const char WindowManagerCore[] = "const.SystemCapability.WindowManager.WindowManager.Core";
        }
        namespace FileManagement
        {
            const char appFileService[] = "const.SystemCapability.FileManagement.AppFileService";
            namespace AppFileService
            {
                const char FolderAuthorization[] = "const.SystemCapability.FileManagement.AppFileService.FolderAuthorization";
            }
            namespace File
            {
                const char DistributedFile[] = "const.SystemCapability.FileManagement.File.DistributedFile";
                const char Environment[] = "const.SystemCapability.FileManagement.File.Environment";
                const char FolderObtain[] = "const.SystemCapability.FileManagement.File.Environment.FolderObtain";
                const char FileIO[] = "const.SystemCapability.FileManagement.File.FileIO";
                const char FileIOLite[] = "const.SystemCapability.FileManagement.File.FileIO.Lite";
            }
            namespace FilePreview
            {
                const char Core[] = "const.SystemCapability.FileManagement.FilePreview.Core";
            }
            namespace StorageService
            {
                const char Backup[] = "const.SystemCapability.FileManagement.StorageService.Backup";
                const char Volume[] = "const.SystemCapability.FileManagement.StorageService.Volume";
                const char Encryption[] = "const.SystemCapability.FileManagement.StorageService.Encryption";
                const char SpatialStatistics[] = "const.SystemCapability.FileManagement.StorageService.SpatialStatistics";
            }
            namespace DistributedFileService
            {
                const char CloudSyncCore[] = "const.SystemCapability.FileManagement.DistributedFileService.CloudSync.Core";
                const char CloudSyncManager[] = "const.SystemCapability.FileManagement.DistributedFileService.CloudSyncManager";
            }
            namespace PhotoAccessHelper
            {
                const char Core[] = "const.SystemCapability.FileManagement.PhotoAccessHelper.Core";
            }
            namespace UserFileManager
            {
                const char Core[] = "const.SystemCapability.FileManagement.UserFileManager.Core";
                const char DistributedCore[] = "const.SystemCapability.FileManagement.UserFileManager.DistributedCore";
            }
            const char userFileService[] = "const.SystemCapability.FileManagement.UserFileService";
            namespace UserFileService
            {
                const char FolderSelection[] = "const.SystemCapability.FileManagement.UserFileService.FolderSelection";
            }
        }
        namespace MultimodalInput
        {
            namespace Input
            {
                const char Cooperator[] = "const.SystemCapability.MultimodalInput.Input.Cooperator";
                const char Core[] = "const.SystemCapability.MultimodalInput.Input.Core";
                const char Pointer[] = "const.SystemCapability.MultimodalInput.Input.Pointer";
                const char ShortKey[] = "const.SystemCapability.MultimodalInput.Input.ShortKey";
                const char InfraredEmitter[] = "const.SystemCapability.MultimodalInput.Input.InfraredEmitter";
                const char InputConsumer[] = "const.SystemCapability.MultimodalInput.Input.InputConsumer";
                const char InputDevice[] = "const.SystemCapability.MultimodalInput.Input.InputDevice";
                const char InputMonitor[] = "const.SystemCapability.MultimodalInput.Input.InputMonitor";
                const char InputSimulator[] = "const.SystemCapability.MultimodalInput.Input.InputSimulator";
            }
        }
        namespace ResourceSchedule
        {
            namespace BackgroundTaskManager
            {
                const char ContinuousTask[] = "const.SystemCapability.ResourceSchedule.BackgroundTaskManager.ContinuousTask";
                const char TransientTask[] = "const.SystemCapability.ResourceSchedule.BackgroundTaskManager.TransientTask";
                const char EfficiencyResourcesApply[] = "const.SystemCapability.ResourceSchedule.BackgroundTaskManager.EfficiencyResourcesApply";
            }
            const char DeviceStandby[] = "const.SystemCapability.ResourceSchedule.DeviceStandby";
            const char SystemLoad[] = "const.SystemCapability.ResourceSchedule.SystemLoad";
            const char WorkScheduler[] = "const.SystemCapability.ResourceSchedule.WorkScheduler";
            const char LowpowerManager[] = "const.SystemCapability.ResourceSchedule.LowpowerManager";
            namespace UsageStatistics
            {
                const char App[] = "const.SystemCapability.ResourceSchedule.UsageStatistics.App";
                const char AppGroup[] = "const.SystemCapability.ResourceSchedule.UsageStatistics.AppGroup";
            }
            const char FfrtCore[] = "const.SystemCapability.Resourceschedule.Ffrt.Core";
        }
        namespace AtomicserviceComponent
        {
            const char UIComponent[] = "const.SystemCapability.AtomicserviceComponent.UIComponent";
            const char atomicservice[] = "const.SystemCapability.AtomicserviceComponent.atomicservice";
        }
        namespace AuthenticationServices
        {
            namespace HuaweiID
            {
                const char Auth[] = "const.SystemCapability.AuthenticationServices.HuaweiID.Auth";
                const char ExtendService[] = "const.SystemCapability.AuthenticationServices.HuaweiID.ExtendService";
                const char MyFamily[] = "const.SystemCapability.AuthenticationServices.HuaweiID.MyFamily";
                const char RetailAuth[] = "const.SystemCapability.AuthenticationServices.HuaweiID.RetailAuth";
                const char UIComponent[] = "const.SystemCapability.AuthenticationServices.HuaweiID.UIComponent";
                const char MinorsProtection[] = "const.SystemCapability.AuthenticationServices.HuaweiID.MinorsProtection";
                const char RealNameVerify[] = "const.SystemCapability.AuthenticationServices.HuaweiID.RealNameVerify";
                const char ShippingAddress[] = "const.SystemCapability.AuthenticationServices.HuaweiID.ShippingAddress";
            }
        }
        namespace DeviceCloudGateway
        {
            namespace ClientCloudCacheService
            {
                const char Grs[] = "const.SystemCapability.DeviceCloudGateway.ClientCloudCacheService.Grs";
            }
            const char CloudCapabilityManager[] = "const.SystemCapability.DeviceCloudGateway.CloudCapabilityManager";
            const char CloudFoundation[] = "const.SystemCapability.DeviceCloudGateway.CloudFoundation";
        }
        namespace UtilityApplication
        {
            const char ParentControlCore[] = "const.SystemCapability.UtilityApplication.ParentControl.Core";
        }
        namespace DistributedDataManager
        {
            namespace CloudSync
            {
                const char Client[] = "const.SystemCapability.DistributedDataManager.CloudSync.Client";
                const char Config[] = "const.SystemCapability.DistributedDataManager.CloudSync.Config";
                const char Server[] = "const.SystemCapability.DistributedDataManager.CloudSync.Server";
            }
            namespace DataShare
            {
                const char Consumer[] = "const.SystemCapability.DistributedDataManager.DataShare.Consumer";
                const char Core[] = "const.SystemCapability.DistributedDataManager.DataShare.Core";
                const char Provider[] = "const.SystemCapability.DistributedDataManager.DataShare.Provider";
            }
            namespace KVStore
            {
                const char Core[] = "const.SystemCapability.DistributedDataManager.KVStore.Core";
                const char DistributedKVStore[] = "const.SystemCapability.DistributedDataManager.KVStore.DistributedKVStore";
            }
            namespace UDMF
            {
                const char Core[] = "const.SystemCapability.DistributedDataManager.UDMF.Core";
            }
            const char CommonType[] = "const.SystemCapability.DistributedDataManager.CommonType";
            namespace DataObject
            {
                const char DistributedObject[] = "const.SystemCapability.DistributedDataManager.DataObject.DistributedObject";
            }
            namespace Preferences
            {
                const char Core[] = "const.SystemCapability.DistributedDataManager.Preferences.Core";
                const char CoreLite[] = "const.SystemCapability.DistributedDataManager.Preferences.Core.Lite";
            }
            namespace RelationalStore
            {
                const char Core[] = "const.SystemCapability.DistributedDataManager.RelationalStore.Core";
            }
        }
        namespace DistributedHardware
        {
            const char DeviceManager[] = "const.SystemCapability.DistributedHardware.DeviceManager";
            const char DistributedHardwareFWK[] = "const.SystemCapability.DistributedHardware.DistributedHardwareFWK";
        }
    }
    namespace Ohos
    {
        namespace Boot
        {
            namespace Time
            {
                const char Kernel[] = "ohos.boot.time.kernel";
                const char Init[] = "ohos.boot.time.init";
                namespace Bms
                {
                    namespace Main
                    {
                        namespace Bundles
                        {
                            const char Ready[] = "ohos.boot.time.bms.main.bundles.ready";
                        }
                    }
                }
                namespace Wms
                {
                    const char Fullscreen[] = "ohos.boot.time.wms.fullscreen.ready";
                    const char Ready[] = "ohos.boot.time.wms.ready";
                }
                namespace Samgr
                {
                    const char Ready[] = "ohos.boot.time.samgr.ready";
                }
                namespace Boot
                {
                    const char Completed[] = "ohos.boot.time.boot.completed";
                }
                namespace HdfDevmgr
                {
                    const char Ready[] = "ohos.boot.time.hdf_devmgr.ready";
                }
                namespace Appspawn
                {
                    const char Started[] = "ohos.boot.time.appspawn.started";
                }
                namespace Useriam
                {
                    const char Fwkready[] = "ohos.boot.time.useriam.fwkready";
                }
                namespace Appfwk
                {
                    const char Ready[] = "ohos.boot.time.appfwk.ready";
                }
                namespace Account
                {
                    const char Ready[] = "ohos.boot.time.account.ready";
                }
                namespace Launcher
                {
                    const char Ready[] = "ohos.boot.time.launcher.ready";
                }
                namespace Lockscreen
                {
                    const char Ready[] = "ohos.boot.time.lockscreen.ready";
                }
                namespace ParamWatcher
                {
                    const char Started[] = "ohos.boot.time.param_watcher.started";
                }
                namespace Renderservice
                {
                    const char Ready[] = "ohos.boot.time.renderservice.ready";
                }
                namespace Bootanimation
                {
                    const char Ready[] = "ohos.boot.time.bootanimation.ready";
                    const char Started[] = "ohos.boot.time.bootanimation.started";
                    const char Finished[] = "ohos.boot.time.bootanimation.finished";
                }
                namespace Avsessionservice
                {
                    const char Ready[] = "ohos.boot.time.avsessionservice.ready";
                }
            }

        }

    }
    namespace BootEvent
    {
        namespace Boot
        {
            const char Completed[] = "bootevent.boot.completed";
        }
    }
    namespace Persist
    {
        namespace Sys
        {
            namespace Hilog
            {
                const char Kmsg[] = "persist.sys.hilog.kmsg.on";
                const char Binary[] = "persist.sys.hilog.binary.on";
                const char BinaryForhota[] = "persist.sys.hilog.binary.forhota.on";
            }
        }
        namespace Hdc
        {
            const char Version[] = "const.hdc.version";
        }
        namespace Web
        {
            namespace Debug
            {
                const char Devtools[] = "web.debug.devtools";
                const char Netlog[] = "web.debug.netlog";
                const char Trace[] = "web.debug.trace";
                namespace StrictsiteIsolation
                {
                    const char Enable[] = "web.debug.strictsiteIsolation.enable";
                }
            }
        }
        const char HdcJdwp[] = "persist.hdc.jdwp";
        namespace Time
        {
            const char AutoRestoreTimerApps[] = "persist.time.auto_restore_timer_apps";
            const char Ntpserver[] = "persist.time.ntpserver";
            const char Timezone[] = "persist.time.timezone";
            const char NtpserverSpecific[] = "persist.time.ntpserver_specific";
            const char AutoTime[] = "persist.time.auto_time";
        }
        namespace Global
        {
            const char TzOverride[] = "persist.global.tz_override";
        }
    }
}

} // namespace Ohos::Constants
