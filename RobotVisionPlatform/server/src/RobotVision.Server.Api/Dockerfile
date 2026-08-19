FROM mcr.microsoft.com/dotnet/sdk:10.0 AS build
WORKDIR /src
COPY server/Directory.Build.props server/
COPY server/src/RobotVision.Server.Contracts/RobotVision.Server.Contracts.csproj server/src/RobotVision.Server.Contracts/
COPY server/src/RobotVision.Server.Api/RobotVision.Server.Api.csproj server/src/RobotVision.Server.Api/
RUN dotnet restore server/src/RobotVision.Server.Api/RobotVision.Server.Api.csproj
COPY server/src server/src
RUN dotnet publish server/src/RobotVision.Server.Api/RobotVision.Server.Api.csproj -c Release -o /app --no-restore

FROM mcr.microsoft.com/dotnet/aspnet:10.0
WORKDIR /app
COPY --from=build /app .
USER $APP_UID
EXPOSE 5080
ENTRYPOINT ["dotnet", "RobotVision.Server.Api.dll"]

