using Microsoft.Extensions.Logging;

namespace RobotVision.Dashboard.Maui;

public static class MauiProgram
{
    /// <summary>MAUI application과 BlazorWebView dependency를 구성합니다.</summary>
    public static MauiApp CreateMauiApp()
    {
        // UseMauiApp은 App 수명주기를 등록하고 DI container를 준비합니다.
        var builder = MauiApp.CreateBuilder().UseMauiApp<App>();
        // Razor component를 native WebView 안에서 실행하는 Blazor Hybrid service입니다.
        builder.Services.AddMauiBlazorWebView();
#if DEBUG
        builder.Services.AddBlazorWebViewDeveloperTools();
        builder.Logging.AddDebug();
#endif
        return builder.Build();
    }
}
