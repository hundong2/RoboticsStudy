namespace RobotVision.Dashboard.Maui;

public partial class App : Application
{
    /// <summary>XAML resource를 초기화하고 첫 화면을 생성합니다.</summary>
    public App()
    {
        InitializeComponent();
        MainPage = new MainPage();
    }
}
