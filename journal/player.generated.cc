// Warning!  This file was generated by running a program (see project |tools|).
// If you change it, the changes will be lost the next time the generator is
// run.  You should change the generator instead.

{
  bool ran = false;
  ran |= RunIfAppropriate<AddVesselToNextPhysicsBubble>(*method);
  ran |= RunIfAppropriate<AdvanceTime>(*method);
  ran |= RunIfAppropriate<AtEnd>(*method);
  ran |= RunIfAppropriate<BubbleDisplacementCorrection>(*method);
  ran |= RunIfAppropriate<BubbleVelocityCorrection>(*method);
  ran |= RunIfAppropriate<CelestialFromParent>(*method);
  ran |= RunIfAppropriate<CurrentTime>(*method);
  ran |= RunIfAppropriate<DeleteLineAndIterator>(*method);
  ran |= RunIfAppropriate<DeletePlugin>(*method);
  ran |= RunIfAppropriate<DeletePluginSerialization>(*method);
  ran |= RunIfAppropriate<DeserializePlugin>(*method);
  ran |= RunIfAppropriate<DirectlyInsertCelestial>(*method);
  ran |= RunIfAppropriate<EndInitialization>(*method);
  ran |= RunIfAppropriate<FetchAndIncrement>(*method);
  ran |= RunIfAppropriate<FlightPlanAppend>(*method);
  ran |= RunIfAppropriate<FlightPlanCreate>(*method);
  ran |= RunIfAppropriate<FlightPlanDelete>(*method);
  ran |= RunIfAppropriate<FlightPlanExists>(*method);
  ran |= RunIfAppropriate<FlightPlanGetFinalTime>(*method);
  ran |= RunIfAppropriate<FlightPlanGetInitialTime>(*method);
  ran |= RunIfAppropriate<FlightPlanGetManoeuvre>(*method);
  ran |= RunIfAppropriate<FlightPlanNumberOfManoeuvres>(*method);
  ran |= RunIfAppropriate<FlightPlanNumberOfSegments>(*method);
  ran |= RunIfAppropriate<FlightPlanRemoveLast>(*method);
  ran |= RunIfAppropriate<FlightPlanRenderedSegment>(*method);
  ran |= RunIfAppropriate<FlightPlanReplaceLast>(*method);
  ran |= RunIfAppropriate<FlightPlanSetFinalTime>(*method);
  ran |= RunIfAppropriate<FlightPlanSetTolerances>(*method);
  ran |= RunIfAppropriate<ForgetAllHistoriesBefore>(*method);
  ran |= RunIfAppropriate<GetBufferDuration>(*method);
  ran |= RunIfAppropriate<GetBufferedLogging>(*method);
  ran |= RunIfAppropriate<GetNavigationFrameParameters>(*method);
  ran |= RunIfAppropriate<GetPlottingFrame>(*method);
  ran |= RunIfAppropriate<GetStderrLogging>(*method);
  ran |= RunIfAppropriate<GetSuppressedLogging>(*method);
  ran |= RunIfAppropriate<GetVerboseLogging>(*method);
  ran |= RunIfAppropriate<HasPrediction>(*method);
  ran |= RunIfAppropriate<HasVessel>(*method);
  ran |= RunIfAppropriate<InitGoogleLogging>(*method);
  ran |= RunIfAppropriate<InsertCelestial>(*method);
  ran |= RunIfAppropriate<InsertOrKeepVessel>(*method);
  ran |= RunIfAppropriate<InsertSun>(*method);
  ran |= RunIfAppropriate<LogError>(*method);
  ran |= RunIfAppropriate<LogFatal>(*method);
  ran |= RunIfAppropriate<LogInfo>(*method);
  ran |= RunIfAppropriate<LogWarning>(*method);
  ran |= RunIfAppropriate<NavballOrientation>(*method);
  ran |= RunIfAppropriate<NewBarycentricRotatingNavigationFrame>(*method);
  ran |= RunIfAppropriate<NewBodyCentredNonRotatingNavigationFrame>(*method);
  ran |= RunIfAppropriate<NewNavigationFrame>(*method);
  ran |= RunIfAppropriate<NewPlugin>(*method);
  ran |= RunIfAppropriate<NumberOfSegments>(*method);
  ran |= RunIfAppropriate<PhysicsBubbleIsEmpty>(*method);
  ran |= RunIfAppropriate<RenderedPrediction>(*method);
  ran |= RunIfAppropriate<RenderedVesselTrajectory>(*method);
  ran |= RunIfAppropriate<SayHello>(*method);
  ran |= RunIfAppropriate<SerializePlugin>(*method);
  ran |= RunIfAppropriate<SetBufferDuration>(*method);
  ran |= RunIfAppropriate<SetBufferedLogging>(*method);
  ran |= RunIfAppropriate<SetPlottingFrame>(*method);
  ran |= RunIfAppropriate<SetPredictionLength>(*method);
  ran |= RunIfAppropriate<SetPredictionLengthTolerance>(*method);
  ran |= RunIfAppropriate<SetPredictionSpeedTolerance>(*method);
  ran |= RunIfAppropriate<SetStderrLogging>(*method);
  ran |= RunIfAppropriate<SetSuppressedLogging>(*method);
  ran |= RunIfAppropriate<SetVerboseLogging>(*method);
  ran |= RunIfAppropriate<SetVesselStateOffset>(*method);
  ran |= RunIfAppropriate<UpdateCelestialHierarchy>(*method);
  ran |= RunIfAppropriate<UpdatePrediction>(*method);
  ran |= RunIfAppropriate<VesselBinormal>(*method);
  ran |= RunIfAppropriate<VesselFromParent>(*method);
  ran |= RunIfAppropriate<VesselNormal>(*method);
  ran |= RunIfAppropriate<VesselTangent>(*method);
  CHECK(ran) << method->DebugString();
}
