
#include "SolversLS.h"

/*Define the namespace*/
namespace ROPTLIB{

	void SolversLS::Run(void)
	{
		Variable *xTemp;
		Vector *gfTemp;
		pre_funs.clear();

		Solvers::Run();

		ChooseLinesearch();

		LSstatus = SUCCESS;
		f1 = Prob->f(x1); nf++;
		f2 = f1;
		Prob->Grad(x1, gf1); ng++;
		ngf0 = sqrt(Mani->Metric(x1, gf1, gf1));
		ngf = ngf0;
		newslope = 0;
		iter = 0;
		if (Debug >= ITERRESULT)
		{
			printf("i:%d,f:%.3e,|gf|:%.3e,\n", iter, f1, ngf);
			timeSeries[iter] = static_cast<double>(getTickCount() - starttime) / CLK_PS;
			funSeries[iter] = f1;
			gradSeries[iter] = ngf;
		}
		bool isstop = IsStopped();

		/*Start the loop*/
		while ((((! isstop) && iter < Max_Iteration) || iter < Min_Iteration) && LSstatus == SUCCESS)
		{
			GetSearchDir(); // Obtain search direction eta1

			initialslope = Mani->Metric(x1, gf1, eta1);
			/*Compute initial step size for the next iteration*/
			InitialStepSize();

			initiallength = stepsize;

			/*If accurate enough, then a fixed stepsize is chosen.*/
			if (ngf / ngf0 < Accuracy)
			{
				stepsize = Finalstepsize;
				f2 = h(); nf++;
				Prob->Grad(x2, gf2); ng++;
			}
			else
			{
				/* Call the specified linesearch algorithm.
				Note that in the linesearch algorithm, we need to obtain
				accepted stepsize, eta2=stepsize*eta1, x2 = R_{x_1}(eta_2), f2 = f(x2), and gf2 = grad f(x_2) */
				if (LineSearch_LS == INPUTFUN)
				{
					if (Prob->GetDomain()->GetIsIntrinsic())
					{
						Vector *exeta1 = Prob->GetDomain()->GetEMPTYEXTR()->ConstructEmpty();
						Prob->GetDomain()->ObtainExtr(x1, eta1, exeta1);
						stepsize = LinesearchInput(x1, exeta1, stepsize, initialslope, Prob);
						delete exeta1;
					}
					else
					{
						stepsize = LinesearchInput(x1, eta1, stepsize, initialslope, Prob);
					}
					stepsize = (stepsize < std::numeric_limits<double>::epsilon()) ? initiallength : stepsize;
					initiallength = stepsize;
					if (!IsPureLSInput)
					{
						SolversLS::LinesearchArmijo();
					}
					else
					{
						f2 = h(); nf++;
						Prob->Grad(x2, gf2); ng++;
					}
				}
				else
				{
					(this->*Linesearch)();
				}
			}

			//Vector *exxix = Mani->GetEMPTYEXTR()->ConstructEmpty();//---
			//Mani->ObtainExtr(x1, gf1, exxix);
			//Mani->ExtrProjection(x2, exxix, exxix);
			//Vector *exxiy = Mani->GetEMPTYEXTR()->ConstructEmpty();
			//Mani->ObtainExtr(x2, gf1, exxiy);
			//Vector *exdif = Mani->GetEMPTYEXTR()->ConstructEmpty();
			//Mani->VectorMinusVector(x1, exxix, exxiy, exdif);
			//std::cout << "||Tpara - Tproj|| / ||Tpara||:" << Mani->Metric(x1, exdif, exdif) / sqrt(Mani->Metric(x1, exxix, exxix)) << std::endl;//---

			/*Output debug information if necessary.*/
			if (LSstatus < SUCCESS && Debug >= FINALRESULT )
			{
				std::cout << "Linesearch fails! LSstatus:" << LSstatusSetnames[LSstatus] << std::endl;
			}

			iter++;

			/*Update the Hessian approximation for quasi-Newton methods 
				or obtain search direction candadite for Riemannian nonlinear conjugate gradient*/
			UpdateData();

			/*norm of the gradient at x2*/
			ngf = sqrt(Mani->Metric(x2, gf2, gf2));

			if (Debug >= ITERRESULT)
			{
				/*Output information*/
				if (iter % OutputGap == 0)
				{
					PrintGenInfo();
					PrintInfo(); // Output information specific to Algorithms
				}
				/*Store debug information in the arrays*/
				timeSeries[iter] = static_cast<double>(getTickCount() - starttime) / CLK_PS;
				funSeries[iter] = f2; gradSeries[iter] = ngf;
			}

			/*Call the function to check whether the stopping criterion is satisfied or not.
			The default function is written in Solvers.h and Solvers.cpp*/
			isstop = IsStopped();

			/*Switch information at x1 and x2*/
			xTemp = x1; x1 = x2; x2 = xTemp;
			gfTemp = gf1; gf1 = gf2; gf2 = gfTemp;
			pre_funs.push_front(f1);
			if (pre_funs.size() > Num_pre_funs && pre_funs.size() > 1)
				pre_funs.pop_back();
			f1 = f2;
		}
		ComTime = static_cast<double>(getTickCount() - starttime) / CLK_PS;
		if (Debug >= ITERRESULT)
			lengthSeries = iter + 1;
		if (Debug >= FINALRESULT)
		{
			printf("Iter:%d,f:%.3e,", iter, f2);
			if (nsubgf != -1)
			{
				printf("nsubgf:%.3e,", nsubgf);
			}
			printf("|gf|:%.3e,|gf|/|gf0|:%.3e,time:%.2e,", ngf, ngf / ngf0, ComTime);
			if (subprobtimes != 0)
			{
				printf("nsubprob:%d,", subprobtimes);
			}
			printf("nf:%d,ng:%d,nR:%d,", nf, ng, nR);
			
			if (nH != 0)
			{
				printf("nH:%d,", nH);
			}
			if (nV != 0)
			{
				printf("nV(nVp):%d(%d),", nV, nVp);
			}
			printf("\n");
		}
		if (nsubgf != -1)
		{
			ngf = nsubgf;
		}
	};
	
	void SolversLS::ChooseLinesearch(void)
	{
		/*If the linesearch condition Armijo-Glodstein condition, then the locking conidition is not necessary and don't output warning.
		If the pair of retraction and vector transport satisfies the locking condition, then output warning.
		If the idea in [Section 4.1, HGA2015] is used, then the locking condition is satisfied and don't output the warning.
		[HGA2015]: Wen Huang, K. A. Gallivan, and P.-A. Absil, A Broyden Class of Quais-Newton Methods for Riemannian Optimization,
		SIAM on Journal Optimization, (25)3, 1660-1685, 2015 */
		if (LineSearch_LS != ARMIJO && !Prob->GetDomain()->GetHasLockCon() && !Prob->GetDomain()->GetHasHHR() && Debug >= FINALRESULT)
		{
			std::cout << "Warning: The locking condition is not satisfied. Line search may fail!" << std::endl;
		}

		/*Choose the linesearch algorithm used in the algorithm*/
		if (LineSearch_LS == ARMIJO)
			Linesearch = &SolversLS::LinesearchArmijo;
		else
		if (LineSearch_LS == WOLFE)
			Linesearch = &SolversLS::LinesearchWolfe;
		else
		if (LineSearch_LS == STRONGWOLFE)
			Linesearch = &SolversLS::LinesearchStrongWolfe;
		else
		if (LineSearch_LS == EXACT)
			Linesearch = &SolversLS::LinesearchExact;
		else
		if (LineSearch_LS == WOLFELP)
			Linesearch = &SolversLS::LinesearchWolfeLipschitz;
		else
		if (LineSearch_LS == INPUTFUN)
		{
			if (LinesearchInput == nullptr)
			{
				std::cout << "Error: linesearch function pointer does not exist!" << std::endl;
				return;
			}
		}
		else
		{
			/*If the line search algorithm is not specified, then use the Armijo-Goldstein condition.*/
			if (Debug >= FINALRESULT)
			{
				std::cout << "Warning: linesearch algorithm does not exist!" << std::endl;
				std::cout << "Use linesearch algorithm with Armijo-Goldstein conditions!" << std::endl;
			}
			Linesearch = &SolversLS::LinesearchArmijo;
		}
	};

	void SolversLS::InitialStepSize(void)
	{
		Vector *s = nullptr, *y = nullptr;
		if (iter == 0)
            stepsize = Initstepsize / ngf;
		else
		{
			switch (InitSteptype)
			{
			case QUADINTMOD:
				stepsize = 1.01 * 2.0 * (f1 - pre_funs.front()) / initialslope;
				stepsize = (stepsize > 1) ? 1 : stepsize;
				break;
			case BBSTEP:
				s = eta2->ConstructEmpty();
				y = eta2->ConstructEmpty();
				/*Since x1 and x2 are swapped, we have the following formula.*/
				Mani->VectorTransport(x2, eta2, x1, eta2, s);
				Mani->VectorTransport(x2, eta2, x1, gf2, y);
				Mani->VectorMinusVector(x2, gf1, y, y);
				stepsize = Mani->Metric(x2, s, s) / Mani->Metric(x2, s, y);
				delete s;
				delete y;
				break;
			case ONESTEP:
				stepsize = 1;
				break;
			case QUADINT:
				stepsize = 2.0 * (f1 - pre_funs.front()) / initialslope;
				break;
			default:
				std::cout << "InitSteptype is incorrect. Use one instead." << std::endl;
				stepsize = 1;
			};
			/*Safeguard for the initial stepsize*/
			stepsize = (stepsize < std::numeric_limits<double>::epsilon()) ? Initstepsize / ngf : stepsize;
		}
	};

	void SolversLS::LinesearchWolfeLipschitz(void)
	{
		LSstatus = SUCCESS;
		double prestepsize = 0;
		while (1)
		{
			f2 = h();
			if (f2 - f1 - LS_alpha * initialslope * stepsize > 0) /*-initialslope = \|g\|_P^2*/
			{
				ZoomLP(prestepsize, stepsize);
				break;
			}
			if (dh() >= LS_beta * initialslope)
			{
				break;
			} else
			{
				prestepsize = stepsize;
				stepsize = (2 * stepsize < Maxstepsize) ? 2 * stepsize : Maxstepsize;

				//std::cout << "f2:" << std::setprecision(16) << f2 << ",f1+xx:" << std::setprecision(16) << f1 + LS_alpha * initialslope * stepsize << ",stepsize:" << stepsize << std::endl;//--
				if (stepsize == Maxstepsize)
					break;
			}
		}
		if (stepsize <= Minstepsize)
		{
			LSstatus = MINSTEPSIZE;
		}
		if (stepsize >= Maxstepsize)
		{
			LSstatus = MAXSTEPSIZE;
		}
	};

	void SolversLS::ZoomLP(double a, double b)
	{
		integer times = 0;
		while (1)
		{
			stepsize = (a + b) / 2;

			if (stepsize < Minstepsize)
			{
				if (Debug >= FINALRESULT)
				{
					std::cout << "Warning: step size reaches the minimum:" << Minstepsize << "!" << std::endl;
				}
				LSstatus = MINSTEPSIZE;
				break;
			}

			f2 = h();
			if (f2 - f1 - LS_alpha * initialslope * stepsize > 0)
			{
				b = stepsize;
			}
			else
			{
				if (dh() >= LS_beta * initialslope)
				{
					break;
				}
				else
				{
					a = stepsize;
					if (times >= 30)
					{
						if (Debug >= FINALRESULT)
						{
							std::cout << "warning: line search algorithm reaches upper bound iterations when finding curvature condition." << std::endl;
						}
						//LSstatus = NOCURVATURE;
						break;
					}
				}
			}
			times++;
		};
	};

	void SolversLS::LinesearchArmijo(void)
	{
		LSstatus = SUCCESS;
		f2 = h();
		double maxpref = f1; 
		std::list<double>::iterator j = pre_funs.begin();
		for (integer i = 0; i < Num_pre_funs && j != pre_funs.end(); i++, j++)
		{
			if (maxpref < *j)
				maxpref = *j;
		}

		// simple backtracking
		if (LS_ratio2 <= LS_ratio1)
		{
			double LS_ratio = LS_ratio1;
			while (maxpref - f2 < -LS_alpha * initialslope * stepsize)
			{
				stepsize *= LS_ratio;
				if (stepsize < Minstepsize)
				{
					if (Debug >= FINALRESULT)
					{
						std::cout << "Warning: step size reaches the minimum:" << Minstepsize << "!" << std::endl;
					}
					LSstatus = MINSTEPSIZE;
					break;
				}
				f2 = h();
			}
			Prob->Grad(x2, gf2); ng++;
			return;
		}
	
		// use polynomial interplation
		double prestepsize = stepsize, prestepsize2 = 0, f2pre = 0;
		if (maxpref - f2 < -LS_alpha * initialslope * stepsize)
		{
			stepsize = -initialslope * prestepsize * prestepsize / 2 / (f2 - f1 - initialslope * prestepsize);
			stepsize = (stepsize < LS_ratio2 * prestepsize) ? stepsize : LS_ratio2 * prestepsize;
			stepsize = (stepsize > LS_ratio1 * prestepsize) ? stepsize : LS_ratio1 * prestepsize;
			f2pre = f2;
			prestepsize2 = prestepsize;
			f2 = h();
			prestepsize = stepsize;
		}

		double a11, a12, a21, a22, b1, b2, c, a = 0, b = 0;
		while (maxpref - f2 < -LS_alpha * initialslope * stepsize)
		{
			a11 = 1.0 / prestepsize / prestepsize; 
			a12 = - 1.0 / prestepsize2 / prestepsize2;
			a21 = -prestepsize2 / prestepsize / prestepsize;
			a22 = prestepsize / prestepsize2 / prestepsize2;
			b1 = f2 - f1 - initialslope * prestepsize;
			b2 = f2pre - f1 - initialslope * prestepsize2;
			c = prestepsize - prestepsize2;
			a = (a11 * b1 + a12 * b2) / c;
			b = (a21 * b1 + a22 * b2) / c;
			stepsize = (-b + sqrt(b * b - 3 * a * initialslope)) / 3 / a;
			stepsize = (stepsize < LS_ratio2 * prestepsize) ? stepsize : LS_ratio2 * prestepsize;
			stepsize = (stepsize > LS_ratio1 * prestepsize) ? stepsize : LS_ratio1 * prestepsize;
			if (stepsize < Minstepsize)
			{
				if (Debug >= FINALRESULT)
				{
					std::cout << "Warning: step size reaches the minimum:" << Minstepsize << "!" << std::endl;
				}
				LSstatus = MINSTEPSIZE;
				break;
			}
			f2pre = f2;
			prestepsize2 = prestepsize;
			f2 = h();
			prestepsize = stepsize;
		}
		Prob->Grad(x2, gf2); ng++;
	};

	void SolversLS::LinesearchExact(void)
	{
		double a1, B, t, dir, s, y, sgf1, sgf2, sf1, sf2, LS_ratio = LS_ratio1;
		double tol = initialslope * 1e-16;
		double minstep = sqrt(std::numeric_limits<double>::epsilon());
		LSstatus = SUCCESS;
		t = 1;                // initial step size
		a1 = 0;				  // initial iterate is 0
		sgf1 = initialslope;  // gradient at initial iterate
		B = fabs(sgf1 / initiallength);   // initial Hessian approximation
		sf1 = f1;

		while (1)
		{   // quasi-Newton for optimizing the scalar function
			dir = - sgf1 / B; // obtain direction - B^{-1} grad
			// start Armijo-Goldstein line search
			t = 1;         // initial step size is 1
			stepsize = a1 + t * dir; sf2 = h();   // evaluete the scalar function at (a1 + t * dir)

			while (sf2 > sf1 + LS_alpha * t * sgf1 * dir)
			{
				t *= LS_ratio;
				stepsize = a1 + t * dir; sf2 = h();
				if (t * LS_ratio < minstep)
				{
					sgf2 = dh();
					if (fabs(sgf2 / initialslope) > 1e-1)
						LSstatus = NONEXACT;
					break;
				}
			}

			if (LSstatus == NONEXACT)
				break;
			s = t * dir;
			if (fabs(s) < minstep)
			{
				sgf2 = dh();
				break;
			}
			sgf2 = dh();
			if (fabs(sgf2 / initialslope) < 1e-6)
				break;
			y = sgf2 - sgf1;
			B = (y / s > 0) ? y / s : B;     // for scalar function, Hessian approximation is uniquely defined by s and y.
			// update
			a1 += t * dir;
			sgf1 = sgf2;
			sf1 = sf2;
		}

		if (stepsize <= Minstepsize)
		{
			LSstatus = MINSTEPSIZE;
		}
		if (stepsize >= Maxstepsize)
		{
			LSstatus = MAXSTEPSIZE;
		}
		f2 = sf2;
		newslope = sgf2;
	};

	double SolversLS::h(void)
	{
		Mani->ScaleTimesVector(x1, stepsize, eta1, eta2);
		Mani->Retraction(x1, eta2, x2); nR++;

		//if (stepsize < 1e-4 && stepsize > 1e-7)
		//{
			//Vector *exeta1 = Mani->GetEMPTYEXTR()->ConstructEmpty();//----
			//Vector *exeta2 = Mani->GetEMPTYEXTR()->ConstructEmpty();//----
			//Variable *diffx = x1->ConstructEmpty();
			//Mani->ObtainExtr(x1, eta1, exeta1);//----
			//Mani->ObtainExtr(x1, eta2, exeta2);//----

			//std::cout << "stepsize:" << stepsize << std::endl;//-----
			//exeta1->Print("exeta1:");//----
			//Mani->VectorMinusVector(x1, x2, x1, diffx);//--
			//Mani->ScaleTimesVector(x1, 1 / stepsize, diffx, diffx);//---
			//diffx->Print("diffx:");//----
		//	delete exeta1;
		//	delete exeta2;
		//	delete diffx;//------------
		//}
		nf++;
		return Prob->f(x2);
	};

	double SolversLS::dh(void)
	{
		Prob->Grad(x2, gf2); ng++;
		Mani->DiffRetraction(x1, eta2, x2, eta1, zeta, true); nV++;
		return Mani->Metric(x2, gf2, zeta);
	};

	// Algorithm 3.5 in NW06
	void SolversLS::LinesearchStrongWolfe(void)
	{
		double prestepsize = 0, fpre = f1, newslopepre = initialslope;
		integer times = 0;
		LSstatus = SUCCESS;
		while (1)
		{
			f2 = h();
			if (f2 > f1 + LS_alpha * stepsize * initialslope)
			{
				Zoom(prestepsize, fpre, newslopepre, stepsize, f2);
				return;
			}
			newslope = dh();
			if (fabs(newslope) <= -LS_beta * initialslope)
			{
				return;
			}
			if (newslope >= 0)
			{
				Zoom(stepsize, f2, newslope, prestepsize, fpre);
				return;
			}
			prestepsize = stepsize;
			fpre = f2;
			newslopepre = newslope;
			if (stepsize == Maxstepsize)
			{
				LSstatus = MAXSTEPSIZE;
				return;
			}
			stepsize = (2 * stepsize < Maxstepsize) ? 2 * stepsize : Maxstepsize;
		}
	};

	void SolversLS::Zoom(double x1, double fx1, double slopex1, double x2, double fx2)
	{
		double xdiff, xincr, xlo = x1, xhi = x2, fxlo = fx1, fxhi = fx2, xloslope = slopex1;

		while (1)
		{
			xdiff = (xhi - xlo);
			xincr = -xloslope * xdiff * xdiff / 2 / (fxhi - (fxlo + xloslope * xdiff));
			stepsize = xlo + xincr;
			f2 = h();
			if (f2 > f1 + LS_alpha * stepsize * initialslope || f2 >= fxlo)
			{
				xhi = stepsize;
				fxhi = f2;
			}
			else
			{
				newslope = dh();
				if (fabs(newslope) <= -LS_beta * initialslope)
				{
					return;
				}
				if (newslope * (xhi - xlo) >= 0)
				{
					xhi = xlo;
					fxhi = fxlo;
				}
				xlo = stepsize;
				fxlo = f2;
				xloslope = newslope;
			}
			if (stepsize <= Minstepsize)
			{
				LSstatus = MINSTEPSIZE;
				return;
			}
		}
	};

	void SolversLS::PrintGenInfo(void)
	{
		Solvers::PrintGenInfo();
		printf("LSstatus:%s,initslope:%.3e,", LSstatusSetnames[LSstatus].c_str(), initialslope);
		if (newslope != 0)
		{
			printf("newslope:%.3e,", newslope);
		}
		printf("initstepsize:%.3e,stepsize:%.3e,", initiallength, stepsize);
	};

	void SolversLS::CheckParams(void)
	{
		Solvers::CheckParams();

		std::string LSALGOnames[LSALGOLENGTH] = { "ARMIJO", "WOLFE", "STRONGWOLFE", "EXACT", "WOLFELP", "INPUTFUN" };
		std::string INITSTEPnames[INITSTEPSIZESETLENGTH] = { "ONESTEP", "BBSTEP", "QUADINT", "QUADINTMOD" };

		char YES[] = "YES";
		char NO[] = "NO";
		char *status;

		std::cout << "LINE SEARCH TYPE METHODS PARAMETERS:" << std::endl;
		status = (LineSearch_LS >= 0 && LineSearch_LS < LSALGOLENGTH) ? YES : NO;
		std::cout << "LineSearch_LS :" << std::setw(15) << LSALGOnames[LineSearch_LS] << "[" << status << "],\t";
		status = (LS_alpha > 0 && LS_alpha < 0.5) ? YES : NO;
		std::cout << "LS_alpha      :" << std::setw(15) << LS_alpha << "[" << status << "]" << std::endl;
		if (LineSearch_LS == WOLFE || LineSearch_LS == STRONGWOLFE || LineSearch_LS == WOLFELP)
		{
			status = (LS_beta > 0 && LS_beta < 1) ? YES : NO;
			std::cout << "LS_beta       :" << std::setw(15) << LS_beta << "[" << status << "],\t";
		}
		else
		{
			status = (LS_ratio1 > 0 && LS_ratio1 <= LS_ratio2) ? YES : NO;
			std::cout << "LS_ratio1     :" << std::setw(15) << LS_ratio1 << "[" << status << "],\t";
			status = (LS_ratio2 >= LS_ratio1 && LS_ratio2 < 1) ? YES : NO;
			std::cout << "LS_ratio2     :" << std::setw(15) << LS_ratio2 << "[" << status << "]" << std::endl;
		}
		status = (Initstepsize > 0) ? YES : NO;
		std::cout << "Initstepsize  :" << std::setw(15) << Initstepsize << "[" << status << "]" << std::endl;
		status = (Minstepsize > 0 && Minstepsize <= Maxstepsize) ? YES : NO;
		std::cout << "Minstepsize   :" << std::setw(15) << Minstepsize << "[" << status << "],\t";
		status = (Maxstepsize > 0 && Maxstepsize >= Minstepsize) ? YES : NO;
		std::cout << "Maxstepsize   :" << std::setw(15) << Maxstepsize << "[" << status << "]" << std::endl;
		status = (Accuracy >= 0 && Accuracy <= 1) ? YES : NO;
		std::cout << "Accuracy      :" << std::setw(15) << Accuracy << "[" << status << "],\t";
		status = YES;
		std::cout << "Finalstepsize :" << std::setw(15) << Finalstepsize << "[" << status << "]" << std::endl;
		status = (Num_pre_funs >= 0) ? YES : NO;
		std::cout << "Num_pre_funs  :" << std::setw(15) << Num_pre_funs << "[" << status << "],\t";
		status = (InitSteptype >= 0 && InitSteptype < INITSTEPSIZESETLENGTH) ? YES : NO;
		std::cout << "InitSteptype  :" << std::setw(15) << INITSTEPnames[InitSteptype] << "[" << status << "]" << std::endl;
		if (LineSearch_LS == INPUTFUN)
		{
			status = YES;
			std::cout << "IsPureLSInput :" << std::setw(15) << IsPureLSInput << "[" << status << "]" << std::endl;
		}
	};

	void SolversLS::UpdateData(void)
	{
	};

	void SolversLS::SetProbX(const Problem *prob, const Variable *initialx)
	{
		Solvers::SetProbX(prob, initialx);
		const Vector *EMPTYETA;
		if (prob->GetDomain()->GetIsIntrinsic())
			EMPTYETA = prob->GetDomain()->GetEMPTYINTR();
		else
			EMPTYETA = prob->GetDomain()->GetEMPTYEXTR();
	};

	void SolversLS::SetDefaultParams()
	{
		Solvers::SetDefaultParams();
		LineSearch_LS = ARMIJO;
		LinesearchInput = nullptr;
		Num_pre_funs = 0;
		LS_alpha = 1e-4;
		LS_beta = 0.999;
		Minstepsize = std::numeric_limits<double>::epsilon();
		Maxstepsize = 1e10;
		LS_ratio1 = 0.1;
		LS_ratio2 = 0.9;
		Initstepsize = 1;
		Accuracy = 0;
		Finalstepsize = 1;
		IsPureLSInput = false;
		LSstatusSetnames = new std::string[LSSTATUSSETLENGTH];
		LSstatusSetnames[NOCURVATURE].assign("NOCURVATURE");
		LSstatusSetnames[MINSTEPSIZE].assign("MINSTEPSIZE");
		LSstatusSetnames[MAXSTEPSIZE].assign("MAXSTEPSIZE");
		LSstatusSetnames[NONEXACT].assign("NONEXACT");
		LSstatusSetnames[LSERROR].assign("LSERROR");
		LSstatusSetnames[SUCCESS].assign("SUCCESS");
	};

	SolversLS::~SolversLS(void)
	{
		delete[] LSstatusSetnames;
	};

	// Algorithm 6.3.1MOD in [DS83]
	void SolversLS::LinesearchWolfe(void)
	{
		double prestepsize, f2pre;
		double stepsizelo, stepsizediff, stepsizeincr, steptemp;
		double flo, fhi;
		integer times = 0;
		double n1, n2, n3, n4, a, b, disc;
		LSstatus = SUCCESS;
		while (1)
		{
			f2 = h();
			if (f2 <= f1 + LS_alpha * stepsize * initialslope)
			{
				newslope = dh();
				if (newslope < LS_beta * initialslope)
				{
					times = 0;
					while (f2 <= f1 + LS_alpha * stepsize * initialslope && newslope < LS_beta * initialslope && stepsize < Maxstepsize)
					{
						prestepsize = stepsize;
						f2pre = f2;
						stepsize = (2 * stepsize < Maxstepsize) ? 2 * stepsize : Maxstepsize;
						f2 = h();
						if (f2 <= f1 + LS_alpha * stepsize * initialslope)
							newslope = dh();
						times++;
						if (times > 10)
							break;
					}
					if (stepsize >= Maxstepsize) // stepsize == Maxstepsize
					{
						Prob->Grad(x2, gf2); ng++;
						LSstatus = MAXSTEPSIZE;
						return;
					}
					if (stepsize != initiallength && f2 > f1 + LS_alpha * stepsize * initialslope)
					{
						stepsizelo = (stepsize < prestepsize) ? stepsize : prestepsize;
						stepsizediff = fabs(prestepsize - stepsize);
						if (stepsize < prestepsize)
						{
							flo = f2;
							fhi = f2pre;
						}
						else
						{
							flo = f2pre;
							fhi = f2;
						}
						times = 0;
						while ((f2 > f1 + LS_alpha * stepsize * initialslope || newslope < LS_beta * initialslope) && stepsizediff >= Minstepsize)
						{
							stepsizeincr = -newslope * stepsizediff * stepsizediff / 2 / (fhi - (flo + newslope * stepsizediff));
							stepsizeincr = (stepsizeincr < 0.2 * stepsizediff) ? 0.2 * stepsizediff : stepsizeincr;
							stepsize = stepsizelo + stepsizeincr;
							f2 = h();
							if (f2 > f1 + LS_alpha * stepsize * initialslope)
							{
								stepsizediff = stepsizeincr;
								fhi = f2;
							}
							else
							{
								newslope = dh();
								if (newslope < LS_beta * initialslope)
								{
									stepsizelo = stepsize;
									stepsizediff -= stepsizeincr;
									flo = f2;
								}
							}
							times++;
							if (times > 10)
								break;
						}
						if (newslope < LS_beta * initialslope)
						{
							f2 = h();
							newslope = dh();
							LSstatus = NOCURVATURE;
							return;
						}
					}
				}
				LSstatus = SUCCESS;
				return;
			}
			if (stepsize <= Minstepsize)
			{
				stepsize = Minstepsize;
				f2 = h();
				newslope = dh();
				LSstatus = MINSTEPSIZE;
				return;
			}
			else
			{
				if (stepsize == initiallength)
					steptemp = -initialslope * initiallength * initiallength / 2 / (f2 - f1 - initialslope * initiallength);
				else
				{
					n1 = 1.0 / stepsize / stepsize;
					n2 = 1.0 / prestepsize / prestepsize;
					n3 = (f2 - f1 - stepsize * initialslope) / (stepsize - prestepsize);
					n4 = (f2pre - f1 - prestepsize * initialslope) / (stepsize - prestepsize);
					a = n1 * n3 - n2 * n4;
					b = -prestepsize * n1 * n3 + stepsize * n2 * n4;
					disc = b * b - 3 * a * initialslope;
					if (fabs(a) < 1e-10)
						steptemp = -initialslope / 2 / b;
					else
						steptemp = (-b + sqrt(disc)) / 3 / a;
					steptemp = (steptemp > 0.5 * stepsize) ? 0.5 * stepsize : steptemp;
				}
				prestepsize = stepsize;
				f2pre = f2;
				stepsize = (steptemp <= 1e-2 * stepsize) ? 1e-2 * stepsize : steptemp;
			}
		}
	};

	void SolversLS::SetParams(PARAMSMAP params)
	{
		QuasiNewton::SetParams(params);
		PARAMSMAP::iterator iter;
		for (iter = params.begin(); iter != params.end(); iter++)
		{
			if (iter->first == static_cast<std::string> ("LineSearch_LS"))
			{
				LineSearch_LS = static_cast<LSAlgo> (static_cast<integer> (iter->second));
			}
			else
			if (iter->first == static_cast<std::string> ("LS_alpha"))
			{
				LS_alpha = iter->second;
			}
			else
			if (iter->first == static_cast<std::string> ("LS_beta"))
			{
				LS_beta = iter->second;
			}
			else
			if (iter->first == static_cast<std::string> ("Minstepsize"))
			{
				Minstepsize = iter->second;
			}
			else
			if (iter->first == static_cast<std::string> ("Maxstepsize"))
			{
				Maxstepsize = iter->second;
			}
			else
			if (iter->first == static_cast<std::string> ("LS_ratio1"))
			{
				LS_ratio1 = iter->second;
			}
			else
			if (iter->first == static_cast<std::string> ("LS_ratio2"))
			{
				LS_ratio2 = iter->second;
			}
			else
			if (iter->first == static_cast<std::string> ("Initstepsize"))
			{
				Initstepsize = iter->second;
			}
			else
			if (iter->first == static_cast<std::string> ("Accuracy"))
			{
				Accuracy = iter->second;
			}
			else
			if (iter->first == static_cast<std::string> ("Finalstepsize"))
			{
				Finalstepsize = iter->second;
			}
			else
			if (iter->first == static_cast<std::string> ("Num_pre_funs"))
			{
				Num_pre_funs = static_cast<integer> (iter->second);
			}
			else
			if (iter->first == static_cast<std::string> ("InitSteptype"))
			{
				InitSteptype = static_cast<InitStepsizeSet> (static_cast<integer> (iter->second));
			}
			else
			if (iter->first == static_cast<std::string> ("IsPureLSInput"))
			{
				IsPureLSInput = ((static_cast<integer> (iter->second)) != 0);
			}
		}
	};
}; /*end of ROPTLIB namespace*/
