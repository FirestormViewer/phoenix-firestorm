FROM mcr.microsoft.com/dotnet/sdk:8.0 AS build
WORKDIR /src
COPY BlazingStorm.Relay.csproj .
RUN dotnet restore
COPY . .
RUN dotnet publish -c Release -o /out --no-restore

FROM mcr.microsoft.com/dotnet/aspnet:8.0
WORKDIR /app
COPY --from=build /out .
ENV ASPNETCORE_URLS=http://+:8080
EXPOSE 8080
ENTRYPOINT ["dotnet", "BlazingStorm.Relay.dll"]
