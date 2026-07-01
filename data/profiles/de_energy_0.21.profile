{
  "actions": [
    {
      "documentation": "Retargets the camera on Earth",
      "gui_path": "/Solar System",
      "identifier": "key.home.target.earth",
      "is_local": false,
      "name": "Focus on Earth",
      "script": "openspace.setPropertyValueSingle('NavigationHandler.OrbitalNavigator.Aim', '');openspace.setPropertyValueSingle('NavigationHandler.OrbitalNavigator.Anchor', 'Earth')openspace.setPropertyValueSingle('NavigationHandler.OrbitalNavigator.RetargetAnchor', nil);"
    },
    {
      "documentation": "Retargets the camera on Moon",
      "gui_path": "/Solar System",
      "identifier": "key.shift.home.target.moon",
      "is_local": false,
      "name": "Focus on Moon",
      "script": "openspace.setPropertyValueSingle('NavigationHandler.OrbitalNavigator.Aim', '');openspace.setPropertyValueSingle('NavigationHandler.OrbitalNavigator.Anchor', 'Moon')openspace.setPropertyValueSingle('NavigationHandler.OrbitalNavigator.RetargetAnchor', nil);"
    },
    {
      "documentation": "Retargets the camera on Sun",
      "gui_path": "/Solar System",
      "identifier": "key.ctrl.home.target.sun",
      "is_local": false,
      "name": "Focus on Sun",
      "script": "openspace.setPropertyValueSingle('NavigationHandler.OrbitalNavigator.Aim', '');openspace.setPropertyValueSingle('NavigationHandler.OrbitalNavigator.Anchor', 'Sun')openspace.setPropertyValueSingle('NavigationHandler.OrbitalNavigator.RetargetAnchor', nil);"
    },
    {
      "documentation": "Planet+Moon Trails TOGGLE",
      "gui_path": "/Solar System",
      "identifier": "key.ctrl.p.planet.moon.trails.toggle",
      "is_local": false,
      "name": "Planet+Moon Trails TOGGLE",
      "script": " openspace.setPropertyValueSingle( 'Scene.EarthTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.EarthTrail.Renderable.Enabled')); openspace.setPropertyValueSingle( 'Scene.MoonTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.MoonTrail.Renderable.Enabled')); openspace.setPropertyValueSingle( 'Scene.MercuryTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.MercuryTrail.Renderable.Enabled')); openspace.setPropertyValueSingle( 'Scene.VenusTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.VenusTrail.Renderable.Enabled')); openspace.setPropertyValueSingle( 'Scene.MarsTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.MarsTrail.Renderable.Enabled')); openspace.setPropertyValueSingle( 'Scene.JupiterTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.JupiterTrail.Renderable.Enabled')); openspace.setPropertyValueSingle( 'Scene.SaturnTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.SaturnTrail.Renderable.Enabled')); openspace.setPropertyValueSingle( 'Scene.UranusTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.UranusTrail.Renderable.Enabled')); openspace.setPropertyValueSingle( 'Scene.NeptuneTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.NeptuneTrail.Renderable.Enabled')); "
    },
    {
      "documentation": "Moon Trails TOGGLE",
      "gui_path": "/Solar System",
      "identifier": "key.ctrl.m.moon.trails.toggle",
      "is_local": false,
      "name": "Moon Trails TOGGLE",
      "script": "openspace.setPropertyValueSingle('Scene.MoonTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.MoonTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.CallistoTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.CallistoTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.EuropaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.EuropaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.GanymedeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.GanymedeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.IoTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.IoTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2010J2Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2010J2Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.ThelxinoeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.ThelxinoeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.EuantheTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.EuantheTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.IocasteTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.IocasteTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2003J16Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2003J16Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.PraxidikeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.PraxidikeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.HarpalykeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.HarpalykeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.MnemeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.MnemeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.HermippeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.HermippeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.ThyoneTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.ThyoneTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.AnankeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.AnankeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.HerseTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.HerseTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.AitneTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.AitneTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.KaleTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.KaleTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.TaygeteTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.TaygeteTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.ChaldeneTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.ChaldeneTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.ErinomeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.ErinomeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.KallichoreTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.KallichoreTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.KalykeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.KalykeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.PasitheeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.PasitheeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2010J1Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2010J1Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.EukeladeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.EukeladeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.ArcheTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.ArcheTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.IsonoeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.IsonoeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.CarmeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.CarmeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2003J5Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2003J5Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.CarpoTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.CarpoTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.LedaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.LedaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.HimaliaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.HimaliaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.LysitheaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.LysitheaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.ElaraTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.ElaraTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.DiaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.DiaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.MetisTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.MetisTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.AdrasteaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.AdrasteaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.AmaltheaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.AmaltheaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.ThebeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.ThebeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2003J12Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2003J12Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S/2003J3Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S/2003J3Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2011J1Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2011J1Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2003J19Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2003J19Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2003J10Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2003J10Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2003J23Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2003J23Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2003J9Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2003J9Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2003J2Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2003J2Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.EuporieTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.EuporieTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2003J18Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2003J18Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.HelikeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.HelikeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.OrthosieTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.OrthosieTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2016J1Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2016J1Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2003J15Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2003J15Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.AoedeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.AoedeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.CallirrhoeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.CallirrhoeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.EurydomeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.EurydomeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.KoreTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.KoreTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.CylleneTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.CylleneTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2011J2Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2011J2Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2017J1Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2017J1Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2003J4Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2003J4Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.PasiphaeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.PasiphaeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.HegemoneTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.HegemoneTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.SinopeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.SinopeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.SpondeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.SpondeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.AutonoeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.AutonoeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.MegacliteTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.MegacliteTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.ThemistoTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.ThemistoTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.DioneTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.DioneTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.EnceladusTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.EnceladusTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.HyperionTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.HyperionTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.IapetusTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.IapetusTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.MimasTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.MimasTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.RheaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.RheaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.TethysTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.TethysTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.TitanTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.TitanTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.AlbiorixTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.AlbiorixTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.BebhionnTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.BebhionnTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.ErriapusTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.ErriapusTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.TarvosTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.TarvosTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.KiviuqTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.KiviuqTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.IjiraqTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.IjiraqTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.PaaliaqTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.PaaliaqTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.SiarnaqTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.SiarnaqTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.TarqeqTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.TarqeqTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.PhoebeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.PhoebeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.SkathiTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.SkathiTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2007S2Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2007S2Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.SkollTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.SkollTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2004S13Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2004S13Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.GreipTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.GreipTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.HyrrokkinTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.HyrrokkinTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.JarnsaxaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.JarnsaxaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.MundilfariTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.MundilfariTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2006S1Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2006S1Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2004S17Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2004S17Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.BergelmirTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.BergelmirTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.NarviTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.NarviTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.SuttungrTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.SuttungrTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.HatiTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.HatiTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2004S12Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2004S12Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.FarbautiTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.FarbautiTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.ThrymrTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.ThrymrTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.AegirTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.AegirTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2007S3Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2007S3Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.BestlaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.BestlaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2004S7Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2004S7Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2006S3Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2006S3Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.FENRIRTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.FENRIRTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.KariTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.KariTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.YmirTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.YmirTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.LogeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.LogeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.FornjotTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.FornjotTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.AtlasTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.AtlasTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.PrometheusTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.PrometheusTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.PandoraTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.PandoraTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.EpimetheusTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.EpimetheusTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.JanusTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.JanusTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.AegaeonTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.AegaeonTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.MethoneTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.MethoneTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.AntheTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.AntheTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.PalleneTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.PalleneTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.TelestoTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.TelestoTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.CalypsoTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.CalypsoTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.HeleneTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.HeleneTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.PolydeucesTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.PolydeucesTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.CordeliaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.CordeliaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.OpheliaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.OpheliaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.BiancaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.BiancaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.CressidaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.CressidaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.DesdemonaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.DesdemonaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.JulietTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.JulietTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.PortiaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.PortiaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.RosalindTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.RosalindTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.CupidTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.CupidTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.BelindaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.BelindaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.PerditaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.PerditaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.PuckTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.PuckTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.MabTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.MabTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.MargaretTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.MargaretTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.FranciscoTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.FranciscoTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.CalibanTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.CalibanTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.StephanoTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.StephanoTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.TrinculoTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.TrinculoTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.SycoraxTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.SycoraxTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.ProsperoTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.ProsperoTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.SetebosTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.SetebosTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.FerdinandTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.FerdinandTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.MirandaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.MirandaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.ArielTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.ArielTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.UmbrielTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.UmbrielTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.TitaniaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.TitaniaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.OberonTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.OberonTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.TritonTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.TritonTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.NaiadTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.NaiadTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.ThalassaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.ThalassaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.DespinaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.DespinaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.GalateaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.GalateaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.LarissaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.LarissaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.S2004N1Trail.Renderable.Enabled', not openspace.getPropertyValue('Scene.S2004N1Trail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.ProteusTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.ProteusTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.HalimedeTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.HalimedeTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.PsamatheTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.PsamatheTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.NesoTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.NesoTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.NereidTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.NereidTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.SaoTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.SaoTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.LaomedeiaTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.LaomedeiaTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.CharonBaryentricTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.CharonBaryentricTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.HydraTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.HydraTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.KerberosTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.KerberosTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.NixTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.NixTrail.Renderable.Enabled')); openspace.setPropertyValueSingle('Scene.StyxTrail.Renderable.Enabled', not openspace.getPropertyValue('Scene.StyxTrail.Renderable.Enabled'));"
    },
    {
      "documentation": "Planet+Moon Labels TOGGLE",
      "gui_path": "/Solar System",
      "identifier": "key.ctrl.l.toggle.planet.moon.labels",
      "is_local": false,
      "name": "Planet+Moon Labels TOGGLE",
      "script": " openspace.setPropertyValueSingle( 'Scene.EarthLabel.Renderable.Enabled', not openspace.getPropertyValue('Scene.EarthLabel.Renderable.Enabled')); openspace.setPropertyValueSingle( 'Scene.MoonLabel.Renderable.Enabled', not openspace.getPropertyValue('Scene.MoonLabel.Renderable.Enabled')); openspace.setPropertyValueSingle( 'Scene.MercuryLabel.Renderable.Enabled', not openspace.getPropertyValue('Scene.MercuryLabel.Renderable.Enabled')); openspace.setPropertyValueSingle( 'Scene.VenusLabel.Renderable.Enabled', not openspace.getPropertyValue('Scene.VenusLabel.Renderable.Enabled')); openspace.setPropertyValueSingle( 'Scene.MarsLabel.Renderable.Enabled', not openspace.getPropertyValue('Scene.MarsLabel.Renderable.Enabled')); openspace.setPropertyValueSingle( 'Scene.JupiterLabel.Renderable.Enabled', not openspace.getPropertyValue('Scene.JupiterLabel.Renderable.Enabled')); openspace.setPropertyValueSingle( 'Scene.SaturnLabel.Renderable.Enabled', not openspace.getPropertyValue('Scene.SaturnLabel.Renderable.Enabled')); openspace.setPropertyValueSingle( 'Scene.UranusLabel.Renderable.Enabled', not openspace.getPropertyValue('Scene.UranusLabel.Renderable.Enabled')); openspace.setPropertyValueSingle( 'Scene.NeptuneLabel.Renderable.Enabled', not openspace.getPropertyValue('Scene.NeptuneLabel.Renderable.Enabled')); openspace.setPropertyValueSingle( 'Scene.PlutoLabel.Renderable.Enabled', not openspace.getPropertyValue('Scene.PlutoLabel.Renderable.Enabled')); "
    },
    {
      "documentation": "Earth Atmosphere TOGGLE",
      "gui_path": "/Solar System",
      "identifier": "key.shift.a.earth.atmosphere.toggle",
      "is_local": false,
      "name": "Earth Atmosphere TOGGLE",
      "script": "openspace.toggleFade('Scene.EarthAtmosphere.Renderable',1);"
    },
    {
      "documentation": "Anchor to Earth",
      "gui_path": "/Solar System",
      "identifier": "key.alt.a.anchor.to.earth",
      "is_local": false,
      "name": "Anchor to Earth",
      "script": " openspace.setPropertyValueSingle('NavigationHandler.OrbitalNavigator.Anchor', 'Earth');"
    },
    {
      "documentation": "Stars TOGGLE",
      "gui_path": "/Misc",
      "identifier": "key.alt.s.stars.toggle",
      "is_local": false,
      "name": "Stars TOGGLE",
      "script": "openspace.toggleFade('Scene.Stars.Renderable.Enabled');"
    },
    {
      "documentation": "Earth+Earth Atm+Stars+Sun TOGGLE",
      "gui_path": "/Misc",
      "identifier": "key.alt.x.earth.earthatm.stars.sun.toggle",
      "is_local": false,
      "name": "Earth+Earth Atm+Stars+Sun TOGGLE",
      "script": "var earth_atm_toggle = await openspace.propertyValue('Scene.EarthAtmosphere.Renderable.Enabled'); var earth_toggle = await openspace.propertyValue('Scene.Earth.Renderable.Enabled'); var stars_toggle = await openspace.propertyValue('Scene.Stars.Renderable.Enabled'); var sun_toggle = await openspace.propertyValue('Scene.SunGlare.Renderable.Enabled'); var earth_stars_sun_toggle = (earth_atm_toggle && earth_toggle && stars_toggle && sun_toggle); if (earth_stars_sun_toggle[1] > 0.1) { openspace.setPropertyValueSingle('Scene.EarthAtmosphere.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.Earth.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.Earth.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.EarthAtmosphere.Renderable.Fade',0); openspace.setPropertyValueSingle('Scene.Stars.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.Stars.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.SunGlare.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.SunGlare.Renderable.Enabled', false); } else { openspace.setPropertyValueSingle('Scene.EarthAtmosphere.Renderable.Enabled', true); openspace.setPropertyValueSingle('Scene.EarthAtmosphere.Renderable.Fade',1,1); openspace.setPropertyValueSingle('Scene.Earth.Renderable.Enabled', true); openspace.setPropertyValueSingle('Scene.Earth.Renderable.Fade',1,1); openspace.setPropertyValueSingle('Scene.Stars.Renderable.Enabled', true); openspace.setPropertyValueSingle('Scene.Stars.Renderable.Fade',1,1); openspace.setPropertyValueSingle('Scene.SunGlare.Renderable.Fade',1,1); openspace.setPropertyValueSingle('Scene.SunGlare.Renderable.Enabled', true); }"
    },
    {
      "documentation": "Darken Milky Way",
      "gui_path": "/Misc",
      "identifier": "key.ctrl.v.darken.milky.way",
      "is_local": false,
      "name": "Dark Milky Way",
      "script": "openspace.setPropertyValueSingle('Scene.AllSky_Visible.Renderable.Opacity', 0.100000);"
    },
    {
      "documentation": "Quick Fade To Black",
      "gui_path": "/Misc",
      "identifier": "key.ctrl.w.quickfadetoblack",
      "is_local": false,
      "name": "Quick Fade TO Black",
      "script": "if openspace.getPropertyValue('RenderEngine.BlackoutFactor') > 0.5 then openspace.setPropertyValueSingle('RenderEngine.BlackoutFactor', 0.0, 1) else openspace.setPropertyValueSingle('RenderEngine.BlackoutFactor', 1.0, 1) end"
    },
    {
      "documentation": "Toggle Earth ESRI VIIIRS Combo/World Imagery",
      "gui_path": "/Misc",
      "identifier": "key.x.toggle.earth.esri.viirs.combo.world.imagery",
      "is_local": false,
      "name": "Toggle ESRIU VIIRS Combo/World Imagery",
      "script": "openspace.setPropertyValueSingle('Scene.Earth.Renderable.Layers.ColorLayers.ESRI_World_Imagery.Enabled', not openspace.getPropertyValue('Scene.Earth.Renderable.Layers.ColorLayers.ESRI_World_Imagery.Enabled')); openspace.setPropertyValueSingle('Scene.Earth.Renderable.Layers.ColorLayers.ESRI_VIIRS_Combo.Enabled', not openspace.getPropertyValue('Scene.Earth.Renderable.Layers.ColorLayers.ESRI_VIIRS_Combo.Enabled'));"
    },
    {
      "documentation": "DMNS Setup",
      "gui_path": "/DMNS Setup",
      "identifier": "key.f12.dmns.setup",
      "is_local": false,
      "name": "DMNS Setup",
      "script": "openspace.setPropertyValue('Scene.*Trail.Renderable.Opacity', 0); openspace.setPropertyValueSingle('RenderEngine.GlobalRotation', [-0.523583,0.000000,0.000000]); openspace.setPropertyValueSingle('RenderEngine.MasterRotation', [0.523583,0.000000,0.000000]); openspace.setPropertyValueSingle('OpenSpaceEngine.PropertyVisibility', 5.000000); openspace.setPropertyValueSingle('Scene.Earth.Renderable.Layers.ColorLayers.ESRI_VIIRS_Combo.Enabled', false); openspace.setPropertyValueSingle('Scene.Earth.Renderable.Layers.ColorLayers.ESRI_World_Imagery.Enabled', true); openspace.sessionRecording.startPlayback((await openspace.absPath('${RECORDINGS}/01.North.America.START.osrectxt'))[1]); openspace.setPropertyValueSingle('Scene.Earth.Renderable.Layers.HeightLayers.Terrain_tileset.Enabled', true); openspace.setPropertyValueSingle('Scene.Earth.Renderable.Layers.HeightLayers.BlendTileLevels', false); openspace.setPropertyValueSingle('Scene.Earth.Renderable.TargetLodScaleFactor', 16.00); openspace.setPropertyValueSingle('Scene.MercuryTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.VenusTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.EarthTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.MarsTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.JupiterTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.SaturnTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.UranusTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.NeptuneTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.MoonTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.IoTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.EuropaTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.GanymedeTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.CallistoTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.PhobosTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.DeimosTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.DioneTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.EnceladusTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.IapetusTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.MimasTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.RheaTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.TethysTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.TitanTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.HyperionTrail.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.SunGlare.Renderable.Fade', 0.550000); openspace.setPropertyValueSingle('Scene.SunGlare.Scale.Scale', 0.500000); openspace.setPropertyValueSingle('Scene.VenusTrail.Renderable.Appearance.Color', [1.000000,0.000000,1.000000]); openspace.setPropertyValueSingle('Scene.EarthTrail.Renderable.Appearance.Color', [0.250000,0.250000,1.000000]); openspace.setPropertyValueSingle('Scene.MarsTrail.Renderable.Appearance.Color', [1.000000,0.250000,0.250000]); openspace.setPropertyValueSingle('Scene.JupiterTrail.Renderable.Appearance.Color', [1.000000,0.750000,0.000000]); openspace.setPropertyValueSingle('Scene.SaturnTrail.Renderable.Appearance.Color', [1.000000,1.000000,0.000000]); openspace.setPropertyValueSingle('Scene.UranusTrail.Renderable.Appearance.Color', [0.750000,1.000000,1.000000]); openspace.setPropertyValueSingle('Scene.NeptuneTrail.Renderable.Appearance.Color', [0.250000,0.410000,1.000000]); openspace.setPropertyValueSingle('Scene.Earth.Renderable.Layers.ColorLayers.RealTimeFires.Fade',0,1); openspace.setPropertyValueSingle('Scene.Earth.Renderable.Layers.ColorLayers.RealTimeFires.Enabled', false); openspace.setPropertyValueSingle('Scene.Earth.Renderable.Layers.ColorLayers.WhalingVoyagesAOL.Fade',0,1); openspace.setPropertyValueSingle('Scene.Earth.Renderable.Layers.ColorLayers.WhalingVoyagesAOL.Enabled', false); openspace.setPropertyValueSingle('Scene.WhalingPortNantucketMASeal.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.WhalingPortNantucketMASeal.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.WhalingPortNewBedfordMASeal.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.WhalingPortNewBedfordMASeal.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.WhalingPortNewLondonCTSeal.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.WhalingPortNewLondonCTSeal.Renderable.Enabled',false); openspace.setPropertyValueSingle('Scene.WhalingPortProvincetownMASeal.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.WhalingPortProvincetownMASeal.Renderable.Enabled',false); openspace.setPropertyValueSingle('Scene.WhalingPortSagHarborNYSeal.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.WhalingPortSagHarborNYSeal.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.WhalingPortSanFranciscoCASeal.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.WhalingPortSanFranciscoCASeal.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.Earth.Renderable.Layers.ColorLayers.us_coal_power_plants_1935_2028.Fade',0,1); openspace.setPropertyValueSingle('Scene.Earth.Renderable.Layers.ColorLayers.us_coal_power_plants_1935_2028.Enabled', false); openspace.setPropertyValueSingle('Scene.Earth.Renderable.Layers.ColorLayers.globalCoalPowerPlants1935_2028.Fade',0,1); openspace.setPropertyValueSingle('Scene.Earth.Renderable.Layers.ColorLayers.globalCoalPowerPlants1935_2028.Enabled',false); openspace.setPropertyValueSingle('Dashboard.Date.Enabled', true); openspace.setPropertyValueSingle('Dashboard.Distance.Enabled', true); openspace.setPropertyValueSingle('Dashboard.Framerate.Enabled', true); openspace.setPropertyValueSingle('Dashboard.GlobeLocation.Enabled', true); openspace.setPropertyValueSingle('Dashboard.ParallelConnection.Enabled', true); openspace.setPropertyValueSingle('Dashboard.Date.FontSize', 10.000000); openspace.setPropertyValueSingle('Dashboard.Date.TimeFormat', 'YYYY/MM/DD'); openspace.setPropertyValueSingle('Scene.bearcreek_cemetery.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.bearcreek_cemetery.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.chs_laurel_refinery__laurel_mt.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.chs_laurel_refinery__laurel_mt.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.craig_station_rooftop_west_view.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.craig_station_rooftop_west_view.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.derby_dome_wy_pump_jack.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.derby_dome_wy_pump_jack.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.fort_st_vrain_generating_station__platteville_co.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.fort_st_vrain_generating_station__platteville_co.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.grass_creek__wy_pump_jack_2.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.grass_creek__wy_pump_jack_2.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.high_wall_mining_collom_pit_colowyo_mine_pano.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.high_wall_mining_collom_pit_colowyo_mine_pano.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.oregon_basin_wy_south_dome_workover_rig.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.oregon_basin_wy_south_dome_workover_rig.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.smith_mine_montana.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.smith_mine_montana.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.tristate_control_room_pano_1.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.tristate_control_room_pano_1.Renderable.Enabled', false); openspace.setPropertyValueSingle('Scene.tristate_erie_substation_pano_5.Renderable.Fade',0,1); openspace.setPropertyValueSingle('Scene.tristate_erie_substation_pano_5.Renderable.Enabled', false); "},
    {
      "documentation": "Toggle Camera Orbit Lock",
      "gui_path": "/Misc",
      "identifier": "key.w.toggle.camera.orbit.lock",
      "is_local": false,
      "name": "Toggle Camera Orbit Lock",
      "script": "if openspace.getPropertyValue('NavigationHandler.OrbitalNavigator.FollowAnchorNodeRotation') then openspace.setPropertyValueSingle('NavigationHandler.OrbitalNavigator.FollowAnchorNodeRotation', false) else openspace.setPropertyValueSingle('NavigationHandler.OrbitalNavigator.FollowAnchorNodeRotation', true) end"
    }
  ],
  "assets": [
    "base",
    "base_keybindings",
    "events/toggle_sun",
    "scene/solarsystem/planets/earth/earth",
    "${ASSETS}/digital_earth/ENERGY_I/COLORADO_ENERGY_GENERATORS/co_generators_bat",
    "${ASSETS}/digital_earth/ENERGY_I/COLORADO_ENERGY_GENERATORS/co_generators_coa",
    "${ASSETS}/digital_earth/ENERGY_I/COLORADO_ENERGY_GENERATORS/co_generators_gas",
    "${ASSETS}/digital_earth/ENERGY_I/COLORADO_ENERGY_GENERATORS/co_generators_hyd",
    "${ASSETS}/digital_earth/ENERGY_I/COLORADO_ENERGY_GENERATORS/co_generators_nuc",
    "${ASSETS}/digital_earth/ENERGY_I/COLORADO_ENERGY_GENERATORS/co_generators_sun",
    "${ASSETS}/digital_earth/ENERGY_I/COLORADO_ENERGY_GENERATORS/co_generators_win",
    "${ASSETS}/digital_earth/ENERGY_I/COAL_GLOBAL_1935-2028/coal_global_1935-2028",
    "${ASSETS}/digital_earth/ENERGY_I/WHALING_AMERICAN_OFFSHORE_LOGS/whaling_1835",
    "${ASSETS}/digital_earth/ENERGY_I/WHALING_PORTS/whaling_ports_map",
    "${ASSETS}/digital_earth/ENERGY_I/SOS_REAL-TIME_FIRES/sos_real_time_fires",
    "${ASSETS}/digital_earth/ENERGY_I/EARTH_AT_NIGHT/black_marble_2012_enhanced",
    "${ASSETS}/digital_earth/ENERGY_I/energy_1_slides/energy_1_slides",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/bearcreek_cemetery/bearcreek_cemetery",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/bearcreek_cemetery/bearcreek_cemetery_360",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/bearcreek_cemetery/bearcreek_cemetery_transforms",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/burnley_mill_steam_engine_1/burnley_mill_steam_engine_1",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/burnley_mill_steam_engine_1/burnley_mill_steam_engine_1_360",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/burnley_mill_steam_engine_1/burnley_mill_steam_engine_1_transforms",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/burnley_mill_steam_engine_2/burnley_mill_steam_engine_2",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/burnley_mill_steam_engine_2/burnley_mill_steam_engine_2_360",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/burnley_mill_steam_engine_2/burnley_mill_steam_engine_2_transforms",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/chs_laurel_refinery__laurel_mt/chs_laurel_refinery__laurel_mt",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/chs_laurel_refinery__laurel_mt/chs_laurel_refinery__laurel_mt_360",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/chs_laurel_refinery__laurel_mt/chs_laurel_refinery__laurel_mt_transforms",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/craig_station_rooftop_west_view/craig_station_rooftop_west_view",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/craig_station_rooftop_west_view/craig_station_rooftop_west_view_360",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/craig_station_rooftop_west_view/craig_station_rooftop_west_view_transforms",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/derby_dome_wy_pump_jack/derby_dome_wy_pump_jack_360",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/derby_dome_wy_pump_jack/derby_dome_wy_pump_jack_4",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/derby_dome_wy_pump_jack/derby_dome_wy_pump_jack_transforms",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/fort_st_vrain_generating_station__platteville_co/fort_st_vrain_generating_station__platteville_co",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/fort_st_vrain_generating_station__platteville_co/fort_st_vrain_generating_station__platteville_co_360",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/fort_st_vrain_generating_station__platteville_co/fort_st_vrain_generating_station__platteville_co_transforms",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/grass_creek__wy_pump_jack_2/grass_creek__wy_pump_jack_2",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/grass_creek__wy_pump_jack_2/grass_creek__wy_pump_jack_2_360",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/grass_creek__wy_pump_jack_2/grass_creek__wy_pump_jack_2_transforms",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/high_wall_mining_collom_pit_colowyo_mine/high_wall_mining_collom_pit_colowyo_mine",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/high_wall_mining_collom_pit_colowyo_mine/high_wall_mining_collom_pit_colowyo_mine_360",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/high_wall_mining_collom_pit_colowyo_mine/high_wall_mining_collom_pit_colowyo_mine_transforms",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/hoover_dam_exterior_intake_towers/hoover_dam_exterior_intake_towers",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/hoover_dam_exterior_intake_towers/hoover_dam_exterior_intake_towers_360",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/hoover_dam_exterior_intake_towers/hoover_dam_exterior_intake_towers_transforms",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/hoover_dam_turbine_room_2/hoover_dam_turbine_room_2",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/hoover_dam_turbine_room_2/hoover_dam_turbine_room_2_360",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/hoover_dam_turbine_room_2/hoover_dam_turbine_room_2_transforms",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/oregon_basin_wy_south_dome_workover_rig/oregon_basin_wy_south_dome_workover_rig",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/oregon_basin_wy_south_dome_workover_rig/oregon_basin_wy_south_dome_workover_rig_360",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/oregon_basin_wy_south_dome_workover_rig/oregon_basin_wy_south_dome_workover_rig_transforms",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/smith_mine_montana/smith_mine_montana",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/smith_mine_montana/smith_mine_montana_360",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/smith_mine_montana/smith_mine_montana_transforms",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/tristate_control_room_pano_1/tristate_control_room_pano_1",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/tristate_control_room_pano_1/tristate_control_room_pano_1_360",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/tristate_control_room_pano_1/tristate_control_room_pano_1_transforms",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/tristate_erie_substation_pano_5/tristate_erie_substation_pano_5",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/tristate_erie_substation_pano_5/tristate_erie_substation_pano_5_360",
    "${ASSETS}/digital_earth/ENERGY_I/PANOS_0.21.2/tristate_erie_substation_pano_5/tristate_erie_substation_pano_5_transforms",
    "${ASSETS}/digital_earth/ENERGY_I/VIDEOS/gridstatus_lmp_screenspacevideo",
    "${ASSETS}/digital_earth/ENERGY_I/VIDEOS/oil_drilling_songs_screenspacevideo",
    "${ASSETS}/digital_earth/ENERGY_I/VIDEOS/sailing_songs_screenspacevideo"
  ],
  "camera": {
    "altitude": 17000000.0,
    "anchor": "Earth",
    "latitude": 58.5877,
    "longitude": 16.1924,
    "type": "goToGeo"
  },
  "delta_times": [
    1.0,
    5.0,
    30.0,
    60.0,
    300.0,
    1800.0,
    3600.0,
    43200.0,
    86400.0,
    604800.0,
    1209600.0,
    2592000.0,
    5184000.0,
    7776000.0,
    15552000.0,
    31536000.0,
    63072000.0,
    157680000.0,
    315360000.0,
    630720000.0
  ],
  "keybindings": [
    {
      "action": "os.solarsystem.FocusEarth",
      "key": "HOME"
    },
    {
      "action": "key.shift.home.target.moon",
      "key": "SHIFT+HOME"
    },
    {
      "action": "key.ctrl.home.target.sun",
      "key": "CTRL+HOME"
    },
    {
      "action": "key.ctrl.p.planet.moon.trails.toggle",
      "key": "CTRL+P"
    },
    {
      "action": "key.ctrl.m.moon.trails.toggle",
      "key": "CTRL+M"
    },
    {
      "action": "key.ctrl.l.toggle.planet.moon.labels",
      "key": "CTRL+L"
    },
    {
      "action": "key.shift.a.earth.atmosphere.toggle",
      "key": "SHIFT+A"
    },
    {
      "action": "key.alt.a.anchor.to.earth",
      "key": "ALT+A"
    },
    {
      "action": "key.alt.s.stars.toggle",
      "key": "ALT+S"
    },
    {
      "action": "key.alt.x.earth.earthatm.stars.sun.toggle",
      "key": "ALT+X"
    },
    {
      "action": "key.ctrl.v.darken.milky.way",
      "key": "CTRL+V"
    },
    {
      "action": "key.ctrl.w.quickfadetoblack",
      "key": "CTRL+W"
    },
    {
      "action": "key.x.toggle.earth.esri.viirs.combo.world.imagery",
      "key": "X"
    },
    {
      "action": "key.f12.dmns.setup",
      "key": "CTRL+F12"
    },
    {
      "action": "key.w.toggle.camera.orbit.lock",
      "key": "W"
    }
  ],
  "mark_nodes": [
    "Earth",
    "Mars",
    "Moon",
    "Sun",
    "Venus",
    "ISS"
  ],
  "meta": {
    "author": "OpenSpace Team",
    "description": "Default OpenSpace Profile. Adds Earth satellites not contained in other profiles.",
    "license": "MIT License",
    "name": "Default",
    "url": "https://www.openspaceproject.com",
    "version": "1.0"
  },
  "properties": [
    {
      "name": "{earth_satellites}.Renderable.Enabled",
      "type": "setPropertyValue",
      "value": "false"
    }
  ],
  "time": {
    "is_paused": false,
    "type": "relative",
    "value": "-1d"
  },
  "version": {
    "major": 1,
    "minor": 4
  }
}
